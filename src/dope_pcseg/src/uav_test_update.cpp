/*
修改了平滑滤波部分，还未测试
*/

#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <pcl_ros/point_cloud.h>

#include <pcl/point_cloud.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/sample_consensus/model_types.h>   
#include <pcl/sample_consensus/method_types.h>   
#include <pcl/segmentation/sac_segmentation.h>  
#include <pcl/filters/extract_indices.h>
#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/sample_consensus/sac_model_perpendicular_plane.h>
#include <pcl/common/angles.h>
#include <pcl/sample_consensus/ransac.h>
// #include <pcl/filters/passthrough.h>

#include <iostream>
#include <vector>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/search/search.h>
#include <pcl/search/kdtree.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/filters/passthrough.h>
#include <pcl/segmentation/region_growing_rgb.h>
#include <pcl/console/print.h>
#include <pcl/console/parse.h>
#include <pcl/console/time.h>
#include <pcl/features/normal_3d.h>



#include <Eigen/Core>
#include <Eigen/StdVector>
#include <Eigen/Geometry>
#include <iostream>
#include <vector>
#include <string>
#include <jsk_recognition_msgs/BoundingBox.h>
#include <jsk_recognition_msgs/BoundingBoxArray.h>
#include <std_msgs/Int32.h>

#include <tf/transform_listener.h>
#include "tf/tf.h"
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/point_cloud_conversion.h>
#include <pcl/common/transforms.h>  
#include <pcl/filters/extract_indices.h>
#include <pcl/segmentation/progressive_morphological_filter.h>
#include <pcl/segmentation/region_growing.h>
#include <visualization_msgs/Marker.h>
#include <queue>
using namespace pcl::console;
using namespace std;
struct Detected_Obj
{
    jsk_recognition_msgs::BoundingBox bounding_box_;

    pcl::PointXYZ min_point_;
    pcl::PointXYZ max_point_;
    pcl::PointXYZ centroid_;
};

class UVA_CLOUD
{
public:
    UVA_CLOUD(){

        // arm_state_cloud = false;
        arm_state_cloud = 0;
        first = false;
        

        find_thing_first = false;
        find_counter = 0;
        // for(int i=0;i<10;i++)
        // {
        //     qx.push(0.0);
        //     qy.push(0.0);
        //     qz.push(0.0);
        // }
        

        pcl_pub_ = nh_.advertise<sensor_msgs::PointCloud2> ("filter_pcl",1);
        pub_bounding_boxs_ = nh_.advertise<jsk_recognition_msgs::BoundingBoxArray> ("bounding_boxs",1);
        vis_pub = nh_.advertise<visualization_msgs::Marker>( "visualization_marker", 0 );

        scan_sub_ = nh_.subscribe("/camera/depth/color/points",10,&UVA_CLOUD::CloudFind,this);
        //segment_pc_array.reserve(5);
    };
    ~UVA_CLOUD(){};

    void CloudFind(const sensor_msgs::PointCloud2::ConstPtr& cloud_msg)
    {
        pcl_output_2.header = cloud_msg->header;
        // pcl_output_2.header.frame_id = "firefly/rgbd/camera_depth_link";
        // std::cout<<cloud_msg->header<<std::endl;
        // if(arm_state_cloud!=0 && arm_state_cloud!=5)
        // {
            
              CloudFindPro(cloud_msg);
   
        // }
            
            
       
            // if(1)
            // {
            //     first = true;
               jsk_recognition_msgs::BoundingBoxArray bbox_array;
                //  std::cout<<obj_list_.size()<<std::endl;
                for (size_t i = 0; i < obj_list_.size(); i++)
                {

                    bbox_array.boxes.push_back(obj_list_[i].bounding_box_);
                }
                // obj_list_.clear();
                bbox_array.header = pcl_output_.header;
                // const std::string frame_id = "firefly/base_link";
                // bbox_array.header.frame_id = frame_id;
                pub_bounding_boxs_.publish(bbox_array);
                obj_list_.clear();
            // }
            // else
            // {
            //     obj_list_.clear();
            // }
        
       

    }
   
    void CloudFindPro(const sensor_msgs::PointCloud2::ConstPtr& cloud_msg)
    {
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloudPro(new pcl::PointCloud<pcl::PointXYZ>);
        pcl::fromROSMsg (*cloud_msg, *cloudPro);
         pcl_output_2.header = cloud_msg->header;
        //  pcl_output_2.header.frame_id = "firefly/rgbd/camera_depth_link";
         CloudPassSeg(cloudPro); // 直通滤波
         //CloudPydown(cloudPro,0.01); //降采样
         CloudGroundSeg(cloudPro,0.05); //Line分割 //0.7675
         CloudPydown(cloudPro,0.01);
         CloudMean(cloudPro);  //距离分段
        //  std::cout<<cloudPro->size()<<std::endl;
          if(cloudPro->size() > 0)
          {
             for(size_t i =0;i<segment_pc_array.size();i++)
            {
                //欧式聚类 寻找3D boudingbox
                if(segment_pc_array[i]->size() > 0)
                {
                    try{
                        CloudEular(segment_pc_array[i],max_cluster_distance_[i],obj_list_);
                    }
                    catch(char *str)
                    {
                        std::cout << str <<std::endl;
                    }
                }
            }
         }
        pcl::toROSMsg(*cloudPro,pcl_output_);
        pcl_output_.header.frame_id = cloud_msg->header.frame_id;
        pcl_output_.header.stamp = cloud_msg->header.stamp;
        pcl_pub_.publish(pcl_output_);



    }
    
    // 降采样
    void CloudPydown(pcl::PointCloud<pcl::PointXYZ>::Ptr in,double leaf_size)
    {
        pcl::PointCloud<pcl::PointXYZ>::Ptr out(new pcl::PointCloud<pcl::PointXYZ>);
        pcl::VoxelGrid<pcl::PointXYZ> filter;
        filter.setInputCloud(in);
        filter.setLeafSize(leaf_size, leaf_size, leaf_size);
        filter.filter(*out);
        out->swap(*in);
    }
    // 直通滤波
    void CloudPassSeg(pcl::PointCloud<pcl::PointXYZ>::Ptr in)
    {
        // 创建滤波器对象
        pcl::PointCloud<pcl::PointXYZ>::Ptr out(new pcl::PointCloud<pcl::PointXYZ>);
        pcl::PassThrough<pcl::PointXYZ> pass;
        pass.setInputCloud (in);
        pass.setFilterFieldName ("z");
        pass.setFilterLimits (0.0, 0.8);
        //pass.setFilterLimitsNegative (true);
        pass.filter (*out);
        out->swap(*in);
    }
    //分割地面(平面) threo:阙值,平面内 threo m内点云全部分割
    void CloudGroundSeg(pcl::PointCloud<pcl::PointXYZ>::Ptr in,double threo)
    {
        pcl::PointCloud<pcl::PointXYZ>::Ptr out(new pcl::PointCloud<pcl::PointXYZ>);
        pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
        pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
        // 创建一个分割方法
        pcl::SACSegmentation<pcl::PointXYZ> seg;
        // 这一句可以选择最优化参数的因子
        seg.setOptimizeCoefficients (true);
        
        seg.setModelType (pcl::SACMODEL_LINE);   //平面模型
        seg.setMethodType (pcl::SAC_RANSAC);    //分割平面模型所使用的分割方法
        seg.setDistanceThreshold (threo);        //设置最小的阀值距离     
        seg.setMaxIterations(100);//迭代次数
        seg.setInputCloud (in);   //设置输入的点云
        seg.segment (*inliers,*coefficients);  

        pcl::ExtractIndices<pcl::PointXYZ> extract;
        extract.setInputCloud (in);
        extract.setIndices (inliers);
        extract.setNegative (true);
        extract.filter (*out);
        out->swap(*in);


       

    }
    //距离分段
    void CloudMean(pcl::PointCloud<pcl::PointXYZ>::Ptr in)
    {
        segment_pc_array.clear();
        for (size_t i = 0; i < 5; i++)
        {
            pcl::PointCloud<pcl::PointXYZ>::Ptr tmp(new pcl::PointCloud<pcl::PointXYZ>);
            segment_pc_array.push_back(tmp);
        }
        // std::cout<<segment_pc_array.size()<<std::endl;
        for (size_t i = 0; i < in->points.size(); i++)
        {
            pcl::PointXYZ current_point;
            current_point.x = in->points[i].x;
            current_point.y = in->points[i].y;
            current_point.z = in->points[i].z;

            float origin_distance = sqrt(pow(current_point.x, 2) + pow(current_point.y, 2));

            // 如果点的距离大于10m, 忽略该点
            if (origin_distance >= 10)
            {
                continue;
            }
            //分段聚类
            if (origin_distance < seg_distance_[0])
            {
                segment_pc_array[0]->points.push_back(current_point);
            }
            else if (origin_distance < seg_distance_[1])
            {
                segment_pc_array[1]->points.push_back(current_point);
            }
            else if (origin_distance < seg_distance_[2])
            {
                segment_pc_array[2]->points.push_back(current_point);
            }
            else if (origin_distance < seg_distance_[3])
            {
                segment_pc_array[3]->points.push_back(current_point);
            }
            else
            {
                segment_pc_array[4]->points.push_back(current_point);
            }
        }

    }
    // 欧式聚类
    void CloudEular(pcl::PointCloud<pcl::PointXYZ>::Ptr in,
            double in_max_cluster_distance,std::vector<Detected_Obj> &obj_list)
    {
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);

        //create 2d pc
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_2d(new pcl::PointCloud<pcl::PointXYZ>);
        pcl::copyPointCloud(*in, *cloud_2d);
        //make it flat
        for (size_t i = 0; i < cloud_2d->points.size(); i++)
        {
            cloud_2d->points[i].z = 0;
        }

        if (cloud_2d->points.size() > 0)
            tree->setInputCloud(cloud_2d);

        std::vector<pcl::PointIndices> local_indices;

        pcl::EuclideanClusterExtraction<pcl::PointXYZ> euclid;
        euclid.setInputCloud(cloud_2d);
        euclid.setClusterTolerance(in_max_cluster_distance);
        euclid.setMinClusterSize(100);
        euclid.setMaxClusterSize(1000); //1000
        euclid.setSearchMethod(tree);
        euclid.extract(local_indices);
        
        int min_z_index = 0;

        for (size_t i = 0; i < local_indices.size(); i++)
        {
            // the structure to save one detected object
            Detected_Obj obj_info;

            float min_x = std::numeric_limits<float>::max();
            float max_x = -std::numeric_limits<float>::max();
            float min_y = std::numeric_limits<float>::max();
            float max_y = -std::numeric_limits<float>::max();
            float min_z = std::numeric_limits<float>::max();
            float max_z = -std::numeric_limits<float>::max();

            for (auto pit = local_indices[i].indices.begin(); pit != local_indices[i].indices.end(); ++pit)
            {
                //fill new colored cluster point by point
                pcl::PointXYZ p;
                p.x = in->points[*pit].x;
                p.y = in->points[*pit].y;
                p.z = in->points[*pit].z;

                obj_info.centroid_.x += p.x;
                obj_info.centroid_.y += p.y;
                obj_info.centroid_.z += p.z;

                if (p.x < min_x)
                    min_x = p.x;
                if (p.y < min_y)
                    min_y = p.y;
                if (p.z < min_z)
                {                
                    min_z = p.z;
                    min_z_index = *pit;
                }
                if (p.x > max_x)
                    max_x = p.x;
                if (p.y > max_y)
                    max_y = p.y;
                if (p.z > max_z)
                    max_z = p.z;
                    
                
            }
            // std::cout<<"minz_p = "<<"minz_index"<<min_z_index<<std::endl;
            // std::cout<<"minz_p = "<<"axis"<<in->points[min_z_index].x
            // <<","<<in->points[min_z_index].y<<","<<in->points[min_z_index].z
            // <<std::endl;

            // 平滑滤波
            float sumx = 0,sumy = 0,sumz = 0;
            if(!find_thing_first) // 填满数组 
            {
                mean_minzz_[10-1] = in->points[min_z_index].z;
                mean_minzy_[10-1] = in->points[min_z_index].y;
                mean_minzx_[10-1] = in->points[min_z_index].x;

                for(size_t i=0;i<10-1;i++)
                {
                    mean_minzx_[i] = mean_minzx_[i+1];
                    mean_minzy_[i] = mean_minzy_[i+1];
                    mean_minzz_[i] = mean_minzz_[i+1];
                }
                find_counter++;
                if(find_counter>=10)
                {
                    find_thing_first = true;
                }
                
                std::cout<<"find but wait data process!"<<std::endl;
        
            }
            else // 均值滤波发出
            {
                for(size_t i=0;i<10;i++)
                {
                    sumx += mean_minzx_[i];
                    sumy += mean_minzy_[i];
                    sumz += mean_minzz_[i];
                }
                sumx /= 10;
                sumy /= 10;
                sumz /= 10;
                std::cout<<"data are sending!"<<std::endl;

            }

            /*
            // first
            mean_minzz_[10-1] = in->points[min_z_index].z;
            mean_minzy_[10-1] = in->points[min_z_index].y;
            mean_minzx_[10-1] = in->points[min_z_index].x;
            float sumx = 0,sumy = 0,sumz = 0;
            sumx += mean_minzx_[10-1];
            sumy += mean_minzy_[10-1];
            sumz += mean_minzz_[10-1];
            for(size_t i=0;i<10-1;i++)
            {
                sumx += mean_minzx_[i];
                sumy += mean_minzy_[i];
                sumz += mean_minzz_[i];
                mean_minzx_[i] = mean_minzx_[i+1];
                mean_minzy_[i] = mean_minzy_[i+1];
                mean_minzz_[i] = mean_minzz_[i+1];

            }
            sumx /= 10;
            sumy /= 10;
            sumz /= 10;
            */

            // 发布坐标
            geometry_msgs::PoseStamped make_msg;
            make_msg.header = pcl_output_2.header;
            make_msg.pose.position.x = sumx;
            make_msg.pose.position.y = sumy;
            make_msg.pose.position.z = sumz;
            make_msg.pose.orientation.x = 0.0;
            make_msg.pose.orientation.y = 0.0;
            make_msg.pose.orientation.z = 0.0;
            make_msg.pose.orientation.w = 1.0;
            ToMaker(make_msg);
            

            //min, max points
            obj_info.min_point_.x = min_x;
            obj_info.min_point_.y = min_y;
            obj_info.min_point_.z = min_z;

            obj_info.max_point_.x = max_x;
            obj_info.max_point_.y = max_y;
            obj_info.max_point_.z = max_z;

            //calculate centroid
            if (local_indices[i].indices.size() > 0)
            {
                obj_info.centroid_.x /= local_indices[i].indices.size();
                obj_info.centroid_.y /= local_indices[i].indices.size();
                obj_info.centroid_.z /= local_indices[i].indices.size();
            }

            //calculate bounding box
            double length_ = obj_info.max_point_.x - obj_info.min_point_.x;
            double width_ = obj_info.max_point_.y - obj_info.min_point_.y;
            double height_ = obj_info.max_point_.z - obj_info.min_point_.z;


            // skywoodsz

            obj_info.bounding_box_.header = pcl_output_2.header;
            // obj_info.bounding_box_.header.frame_id = "firefly/base_link"; 

            // obj_info.bounding_box_.header.frame_id = "firefly/rgbd/camera_depth_link";

            obj_info.bounding_box_.pose.position.x = obj_info.min_point_.x + length_ / 2;
            obj_info.bounding_box_.pose.position.y = obj_info.min_point_.y + width_ / 2;
            obj_info.bounding_box_.pose.position.z = obj_info.min_point_.z + height_ / 2;

            obj_info.bounding_box_.dimensions.x = ((length_ < 0) ? -1 * length_ : length_);
            obj_info.bounding_box_.dimensions.y = ((width_ < 0) ? -1 * width_ : width_);
            obj_info.bounding_box_.dimensions.z = ((height_ < 0) ? -1 * height_ : height_);

            // obj_info.bounding_box_.pose.position.x = in->points[min_z_index].x;
            // obj_info.bounding_box_.pose.position.y = in->points[min_z_index].y;
            // obj_info.bounding_box_.pose.position.z = in->points[min_z_index].z;

            // obj_info.bounding_box_.dimensions.x = 0.1;
            // obj_info.bounding_box_.dimensions.y = 0.1;
            // obj_info.bounding_box_.dimensions.z = 0.1;

            geometry_msgs::PoseStamped bounding_box_postion,bounding_box_warld_pose;
            bounding_box_postion.header = obj_info.bounding_box_.header;
            bounding_box_postion.header.stamp = ros::Time(0);
            bounding_box_postion.pose.position = obj_info.bounding_box_.pose.position;
            bounding_box_postion.pose.orientation.x = 0.0;
            bounding_box_postion.pose.orientation.y = 0.0;
            bounding_box_postion.pose.orientation.z = 0.0;
            bounding_box_postion.pose.orientation.w = 1.0;

            // double temp_y,temp_z,temp_x;
            // temp_x = obj_info.bounding_box_.dimensions.x;
            // temp_y = obj_info.bounding_box_.dimensions.y;
            // temp_z = obj_info.bounding_box_.dimensions.z;

            // obj_info.bounding_box_.dimensions.x = temp_z;
            // obj_info.bounding_box_.dimensions.y = temp_x;
            // obj_info.bounding_box_.dimensions.z = temp_y;


            obj_list.push_back(obj_info);
        }

    }
    void ToMaker(const geometry_msgs::PoseStamped& msg)
    {
        visualization_msgs::Marker marker;
        // marker.header.frame_id = pcl_output_2.header.frame_id;
        // marker.header.stamp = ros::Time();
        marker.header = pcl_output_2.header;
        marker.ns = "camera";
        marker.id = 0;
        marker.type = visualization_msgs::Marker::SPHERE;
        marker.action = visualization_msgs::Marker::ADD;
        marker.pose.position.x = msg.pose.position.x;
        marker.pose.position.y = msg.pose.position.y;
        marker.pose.position.z = msg.pose.position.z;
        marker.pose.orientation.x = msg.pose.orientation.x;
        marker.pose.orientation.y = msg.pose.orientation.y;
        marker.pose.orientation.z = msg.pose.orientation.z;
        marker.pose.orientation.w = msg.pose.orientation.w;
        marker.scale.x = 0.05;
        marker.scale.y = 0.05;
        marker.scale.z = 0.05;
        marker.color.a = 1.0; // Don't forget to set the alpha!
        marker.color.r = 0.0;
        marker.color.g = 1.0;
        marker.color.b = 0.0;
        
        vis_pub.publish( marker );

    }
private:
   ros::NodeHandle nh_;
   ros::Subscriber scan_sub_;
   ros::Subscriber arm_sub_;
   ros::Publisher pcl_pub_;
   ros::Publisher pub_bounding_boxs_;
   ros::Publisher vis_pub; 

   sensor_msgs::PointCloud2 pcl_output_,pcl_output_2;
   std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> segment_pc_array;
   double seg_distance_[5] = {2,4,6,8,10};
//    double max_cluster_distance_[5] = {0.05,0.2,0.5,0.75,1.0};
   double max_cluster_distance_[5] = {0.05,0.1,0.2,0.4,0.8};
   std::vector<Detected_Obj> obj_list_;

   int arm_state_cloud;
   bool first;

   pcl::PointCloud<pcl::PointXYZ> cloud;
   tf::TransformListener listener_;
   queue<float> qx,qy,qz;
   // 寻找异物
   bool find_thing_first;
   int find_counter;
   float mean_minzx_[10] = {0},mean_minzy_[10] = {0},mean_minzz_[10] = {0};

};

int main(int argc, char *argv[])
{
    ros::init(argc, argv, "cloud");
    UVA_CLOUD uva_cloud;
    ros::spin();
    return 0;
}
