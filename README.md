# Line_Seg
依赖安装：  
	1. jsk_recognition：https://github.com/jsk-ros-pkg/jsk_recognition  
	2. eigen3  
	3. pcl_ros  
	4. realsense: https://github.com/IntelRealSense/realsense-ros  
run:  
	1. 获得点云  
	roslaunch realsense2_camera rs_camera.launch filters:=pointcloud  
	
	2. 分割获取定位  
	rosrun dope_pcseg uav_test  
	
	3. 定位点话题：  
	visualization_msgs/Marker 类型：  
	visualization_marker  
	
	3dbbox 类型：  
	bounding_boxs  

	分割点云：  
	filter_pcl  
	
