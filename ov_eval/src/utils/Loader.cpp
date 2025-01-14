/*
 * OpenVINS: An Open Platform for Visual-Inertial Research
 * Copyright (C) 2019 Patrick Geneva
 * Copyright (C) 2019 Kevin Eckenhoff
 * Copyright (C) 2019 Guoquan Huang
 * Copyright (C) 2019 OpenVINS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "Loader.h"


using namespace ov_eval;


void Loader::load_data(std::string path_traj,
                       std::vector<double> &times, std::vector<Eigen::Matrix<double,7,1>> &poses,
                       std::vector<Eigen::Matrix3d> &cov_ori, std::vector<Eigen::Matrix3d> &cov_pos) {

    // Try to open our trajectory file
    std::ifstream file(path_traj);
    if(!file.is_open()) {
        ROS_ERROR("[LOAD]: Unable to open trajectory file...");
        ROS_ERROR("[LOAD]: %s",path_traj.c_str());
        std::exit(EXIT_FAILURE);
    }

    // Loop through each line of this file
    std::string current_line;
    while(std::getline(file, current_line) && ros::ok()) {

        // Skip if we start with a comment
        if(!current_line.find("#"))
            continue;

        // Loop variables
        int i = 0;
        std::istringstream s(current_line);
        std::string field;
        Eigen::Matrix<double,20,1> data;

        // Loop through this line (timestamp(s) tx ty tz qx qy qz qw Pr11 Pr12 Pr13 Pr22 Pr23 Pr33 Pt11 Pt12 Pt13 Pt22 Pt23 Pt33)
        while(std::getline(s,field,' ') && ros::ok()) {
            // Skip if empty
            if(field.empty() || i >= data.rows())
                continue;
            // save the data to our vector
            data(i) = std::atof(field.c_str());
            i++;
        }

        // Only a valid line if we have all the parameters
        if(i >= 20) {
            // time and pose
            times.push_back(data(0));
            poses.push_back(data.block(1,0,7,1));
            // covariance values
            Eigen::Matrix3d c_ori, c_pos;
            c_ori << data(8),data(9),data(10),
                    data(9),data(11),data(12),
                    data(10),data(12),data(13);
            c_pos << data(14),data(15),data(16),
                    data(15),data(17),data(18),
                    data(16),data(18),data(19);
            c_ori = 0.5*(c_ori+c_ori.transpose());
            c_pos = 0.5*(c_pos+c_pos.transpose());
            cov_ori.push_back(c_ori);
            cov_pos.push_back(c_pos);
        } else if(i >= 8) {
            times.push_back(data(0));
            poses.push_back(data.block(1,0,7,1));
        }

    }

    // Finally close the file
    file.close();

    // Error if we don't have any data
    if (times.empty()) {
        ROS_ERROR("[LOAD]: Could not parse any data from the file!!");
        ROS_ERROR("[LOAD]: %s",path_traj.c_str());
        std::exit(EXIT_FAILURE);
    }

    // Assert that they are all equal
    if(times.size() != poses.size()) {
        ROS_ERROR("[LOAD]: Parsing error, pose and timestamps do not match!!");
        ROS_ERROR("[LOAD]: %s",path_traj.c_str());
        std::exit(EXIT_FAILURE);
    }

    // Assert that they are all equal
    if(!cov_ori.empty() && (times.size() != cov_ori.size() || times.size() != cov_pos.size())) {
        ROS_ERROR("[LOAD]: Parsing error, timestamps covariance size do not match!!");
        ROS_ERROR("[LOAD]: %s",path_traj.c_str());
        std::exit(EXIT_FAILURE);
    }

    // Debug print amount
    //std::string base_filename = path_traj.substr(path_traj.find_last_of("/\\") + 1);
    //ROS_INFO("[LOAD]: loaded %d poses from %s",(int)poses.size(),base_filename.c_str());

}



void Loader::load_simulation(std::string path, std::vector<Eigen::VectorXd> &values) {

    // Try to open our trajectory file
    std::ifstream file(path);
    if(!file.is_open()) {
        ROS_ERROR("[LOAD]: Unable to open file...");
        ROS_ERROR("[LOAD]: %s",path.c_str());
        std::exit(EXIT_FAILURE);
    }

    // Loop through each line of this file
    std::string current_line;
    while(std::getline(file, current_line) && ros::ok()) {

        // Skip if we start with a comment
        if(!current_line.find("#"))
            continue;

        // Loop variables
        std::istringstream s(current_line);
        std::string field;
        std::vector<double> vec;

        // Loop through this line (timestamp(s) values....)
        while(std::getline(s,field,' ') && ros::ok()) {
            // Skip if empty
            if(field.empty())
                continue;
            // save the data to our vector
            vec.push_back(std::atof(field.c_str()));
        }

        // Create eigen vector
        Eigen::VectorXd temp(vec.size());
        for(size_t i=0; i<vec.size(); i++) {
            temp(i) = vec.at(i);
        }
        values.push_back(temp);

    }

    // Finally close the file
    file.close();

    // Error if we don't have any data
    if (values.empty()) {
        ROS_ERROR("[LOAD]: Could not parse any data from the file!!");
        ROS_ERROR("[LOAD]: %s",path.c_str());
        std::exit(EXIT_FAILURE);
    }

    // Assert that all rows in this file are of the same length
    int rowsize = values.at(0).rows();
    for(size_t i=0; i<values.size(); i++) {
        if(values.at(i).rows() != rowsize) {
            ROS_ERROR("[LOAD]: Invalid row size on line %d (of size %d instead of %d)",(int)i,(int)values.at(i).rows(),rowsize);
            ROS_ERROR("[LOAD]: %s",path.c_str());
            std::exit(EXIT_FAILURE);
        }
    }

}



double Loader::get_total_length(const std::vector<Eigen::Matrix<double,7,1>> &poses) {

    // Loop through every pose and append its segment
    double distance = 0.0;
    for (size_t i=1; i<poses.size(); i++) {
        distance += (poses[i] - poses[i-1]).norm();
    }

    // return the distance
    return distance;

}


