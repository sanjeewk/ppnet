/*
 * Copyright 2019 Xilinx Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <csignal>
#include <execinfo.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <future>
#include <vitis/ai/pointpillars.hpp>
#include <opencv2/opencv.hpp>
#include <vitis/ai/profiling.hpp>
#include <chrono>
#include <ctime>

using namespace std;
using namespace cv;
using namespace vitis::ai;

void get_display_data(DISPLAY_PARAM &);

namespace vitis
{
    namespace ai
    {
        extern cv::Mat bev_preprocess(const V1F &PointCloud);
    }
} // namespace vitis

inline cv::Mat wrap_imread(const std::string &fn)
{
    return cv::imread(fn);
}

template <typename T>
void myreadfile(T *dest, int size1, std::string filename)
{
    ifstream Tin;
    Tin.open(filename, ios_base::in | ios_base::binary);
    if (!Tin)
    {
        cout << "Can't open the file! " << filename << std::endl;
        return;
    }
    Tin.read((char *)dest, size1 * sizeof(T));
}

int getfloatfilelen(const std::string &file)
{
    struct stat statbuf;
    if (stat(file.c_str(), &statbuf) != 0)
    {
        std::cerr << " bad file stat " << file << std::endl;
        exit(-1);
    }
    return statbuf.st_size / 4;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        std::cout << "usage: " << argv[0] << " model0_name model1_name bin_file pic_file \n"
                  << "         model0_name can be: pointpillars_kitti_12000_0_pt"
                  << "         model1_name can be: pointpillars_kitti_12000_1_pt"
                  << "\n";
        abort();
    }
    auto net = vitis::ai::PointPillars::create(argv[1], argv[2]);

    DISPLAY_PARAM g_test;
    get_display_data(g_test);

    // int flag = E_RGB;
    // int flag = E_BEV;
    int flag = E_BEV | E_RGB;

    std::string lidar_file(argv[3]);
    std::string image_file(argv[4]);

    std::array<string, 108> lidar_path_list;
    std::array<string, 108> image_path_list;

    ifstream readLidar(lidar_file);
    ifstream readImage(image_file);

    string lidar;
    string image;

    int c=0;

    while (getline(readLidar, lidar)){
        lidar_path_list[c] = lidar;
        cout << lidar;
        c+=1;
    }

    c = 0;
    while (getline(readImage, image)){
        image_path_list[c] = image;
        cout << image;
        c+=1;
    }
    std::cout <<"------------------------------------------";
    // cout << image_path_list;
    for (auto v : image_path_list)
        std::cout << v << "\n";
    // cout << lidar_path_list;
    std::cout <<"------------------------------------------";
    for (auto v : lidar_path_list)
        std::cout << v << "\n";
    std::cout <<"------------------------------------------";
    cv::Mat rgbmat;
    cv::Mat bevmat;
    std::future<cv::Mat> fut_rgb;

    double total_time = 0.0;

    for (int i = 0, j = 0; i < 108, j < 108; i++, j++)
    {
        std::string lidar_path = lidar_path_list.at(i);
        std::string image_path = image_path_list.at(j);

        __TIC__(readfile_cloud_vec2)
        V1F PointCloud;
        int len = getfloatfilelen(lidar_path);
        PointCloud.resize(len);
        myreadfile(PointCloud.data(), len, lidar_path);
        __TOC__(readfile_cloud_vec2)

        std::future<cv::Mat> fut_bev;
        if (flag & E_BEV)
        {
            fut_bev = std::async(std::launch::async, vitis::ai::bev_preprocess, std::cref(PointCloud));
        }

        //  if (flag & E_BEV) {
        //    bevmat = fut_bev.get();
        //    // cv::imwrite("~/bev0.jpg", bevmat);
        //  }

        __TIC__(rgb_read)
        rgbmat = cv::imread(image_path);
        __TOC__(rgb_read)

        // debug : move to fronter place for debug
        if (flag & E_BEV)
        {
            bevmat = fut_bev.get();
            // cv::imwrite("~/bev0.jpg", bevmat);
        }

        __TIC__(result_show)
        ANNORET annoret;

        auto start = std::chrono::system_clock::now();
        auto res = net->run(PointCloud);
        
        net->do_pointpillar_display(res, flag, g_test, rgbmat, bevmat, rgbmat.cols, rgbmat.rows, annoret);
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;

        total_time+=elapsed_seconds.count();

        __TOC__(result_show)

        std::string save_path = image_path.substr(0, image_path.find_last_of('.'));
        if (flag & E_RGB)
        {
            std::cout<<"saving image";              
            cv::imwrite(save_path + "_rgb.jpg", rgbmat);
            //cv::imshow("", rgbmat);
            // waitKey(0);
        }

        if (flag & E_BEV)
        {
            cv::imwrite(save_path + "_bev.jpg", bevmat);
            //cv::imshow("", bevmat);
            //waitKey(0);
        }

        // print result
        for (unsigned int i = 0; i < res.ppresult.final_box_preds.size(); i++)
        {
            std::cout << res.ppresult.label_preds[i] << "     " << std::fixed << std::setw(11) << std::setprecision(6) << std::setfill(' ')
                      << res.ppresult.final_box_preds[i][0] << " "
                      << res.ppresult.final_box_preds[i][1] << " "
                      << res.ppresult.final_box_preds[i][2] << " "
                      << res.ppresult.final_box_preds[i][3] << " "
                      << res.ppresult.final_box_preds[i][4] << " "
                      << res.ppresult.final_box_preds[i][5] << " "
                      << res.ppresult.final_box_preds[i][6] << "     "
                      << res.ppresult.final_scores[i] << "\n";
        }
        //   V2F final_box_preds;
        //     V1F final_scores;
        //       V1I label_preds;
        //
    }
    double avg_time = total_time/108.0;
    cout << "--------------------------------------------------------------------------------------";
    cout << "\n";
    cout << avg_time;
    return 0;
}

void get_display_data(DISPLAY_PARAM &g_v)
{
    g_v.P2.emplace_back(std::vector<float>{721.54, 0, 609.56, 44.857});
    g_v.P2.emplace_back(std::vector<float>{0, 721.54, 172.854, 0.21638});
    g_v.P2.emplace_back(std::vector<float>{0, 0, 1, 0.002746});
    g_v.P2.emplace_back(std::vector<float>{0, 0, 0, 1});

    g_v.rect.emplace_back(std::vector<float>{0.999924, 0.009838, -0.007445, 0});
    g_v.rect.emplace_back(std::vector<float>{-0.00987, 0.99994, -0.00427846, 0});
    g_v.rect.emplace_back(std::vector<float>{0.007403, 0.004351614, 0.999963, 0});
    g_v.rect.emplace_back(std::vector<float>{0, 0, 0, 1});

    g_v.Trv2c.emplace_back(std::vector<float>{0.007534, -0.99997, -0.0006166, -0.00407});
    g_v.Trv2c.emplace_back(std::vector<float>{0.0148, 0.000728, -0.99989, -0.07632});
    g_v.Trv2c.emplace_back(std::vector<float>{0.99986, 0.0075238, 0.0148, -0.27178});
    g_v.Trv2c.emplace_back(std::vector<float>{0, 0, 0, 1});

    g_v.p2rect.resize(4);
    for (int i = 0; i < 4; i++)
    {
        g_v.p2rect[i].resize(4);
        for (int j = 0; j < 4; j++)
        {
            for (int k = 0; k < 4; k++)
            {
                g_v.p2rect[i][j] += g_v.P2[i][k] * g_v.rect[k][j];
            }
        }
    }
    return;
}
