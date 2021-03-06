#include <sstream>
#include <iostream>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <deque>
#include <ctime>
#include <queue>
#include <string>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <libgen.h>
#include <grp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <opencv2/opencv.hpp>
#define BUFSIZE 1024
#define DISK_MAX 0
#define DISK_AVA 1
#define BY_MINUTE 0
#define BY_SECOND 1
#define VID_TIME time_t
#define FOL_TIME time_t
#define DEBUG
#define ll long long
using namespace std;
using namespace cv;

queue<string> folder_list;
queue<string> video_list;
const char path[100] = "/home/nvidia/DKblackbox/";
string base_path = "/home/dokyun/DKVScode/Blackbox";

namespace patch
{
	template < typename T > std::string to_string(const T& n)
	{
		std::ostringstream stm;
		stm << n;
		return stm.str();
	}
}

string Init_Pipeline(int width, int height, double fps)
{
	return "nvarguscamerasrc ! video/x-raw(memory:NVMM), width=(int)" + \
		patch::to_string(width) + ", height=(int)" + patch::to_string(height)\
		+ ",format=(string)NV12, framerate=(fraction)" + patch::to_string(fps) + \
		"/1 ! nvvidconv ! video/x-raw, format=(string)BGRx ! videoconvert ! video/x-raw,\
	       format=(string)BGR ! appsink";
}


extern ll Get_Disk_Size(int what_size)
{
	struct statfs stf;
	statfs("/", &stf);
	if (what_size == DISK_MAX) return stf.f_bsize * stf.f_blocks;
	else return stf.f_bsize * stf.f_bavail;
}

extern void TimetoString(time_t timer, char *str, int time_type)
{
	struct tm* tmptr;
	tmptr = localtime(&timer);

	if (time_type == BY_SECOND) {
		sprintf(str, "%d%02d%02d_%02d%02d%02d",
			tmptr->tm_year + 1900, tmptr->tm_mon + 1, tmptr->tm_mday,
			tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec);
	}
	else if (time_type == BY_MINUTE) {
		sprintf(str, "%d%02d%02d_%02d%02d",
			tmptr->tm_year + 1900, tmptr->tm_mon + 1, tmptr->tm_mday,
			tmptr->tm_hour, tmptr->tm_min);
	}
}

string Video_Date_Name()
{
	char fol[1024];
	char vid[1024];
	char base_path[1024] = "/home/nvidia/DKblackbox/";

	for (int i = 0; i < folder_list.front().size(); i++)
		fol[i] = folder_list.front().at(i);

	for (int i = 0; i < video_list.front().size(); i++)
		vid[i] = video_list.front().at(i);

	strcat(base_path, fol);
	strcat(base_path, "/");
	strcat(base_path, vid);
	strcat(base_path, ".avi");

	return base_path;
}

int Create_Folder(time_t timer)
{
	char fol[100];
	char vid[100];

	TimetoString(timer, fol, BY_MINUTE);
	folder_list.push(fol);

	if (video_list.size() > 1) {
		video_list.pop();
		TimetoString(timer, vid, BY_SECOND);
		video_list.push(vid);
	}
	else {
		TimetoString(timer, vid, BY_SECOND);
		video_list.push(vid);
	}

	if (mkdir(fol, 0776) < 0 && errno != EEXIST) {                //$$$$$$$$$$$$$
		perror("Create Folder Fail\n");
		return -1;
	}

	return 0;
}

int Remove_Dir(const char *path, int is_error_stop)
{
	DIR *dir_ptr = NULL;
	struct dirent *file = NULL;
	struct stat buf;
	char filename[1024];

	if ((dir_ptr = opendir(path)) == NULL) {
		return unlink(path);
	}

	while ((file = readdir(dir_ptr)) != NULL) {

		if (file->d_name[0] == '.')
			continue;

		sprintf(filename, "%s/%s", path, file->d_name);

		if (lstat(filename, &buf) == -1)
			continue;

		if (S_ISDIR(buf.st_mode)) {
			if (Remove_Dir(filename, is_error_stop) == -1 && is_error_stop) {
				return -1;
			}
		}
		else if (S_ISREG(buf.st_mode) || S_ISLNK(buf.st_mode)) {
			if (unlink(filename) == -1 && is_error_stop)
				return -1;
		}
	}
	closedir(dir_ptr);
	return rmdir(path);
}

int Video_Record()
{
	Mat img;

	ll max_size = Get_Disk_Size(DISK_MAX);
	ll ava_size = Get_Disk_Size(DISK_AVA);
	ll limit = max_size * 0.35;

	//비디오 캡쳐 초기화
	VideoCapture cap(Init_Pipeline(640, 480, 30.0));
	if (!cap.isOpened()) {
		cerr << "cap opened is fail" << endl;
		return -1;
	}

	// 동영상 파일을 저장하기 위한 준비
	// Size size = Size((int)cap.get(CAP_PROP_FRAME_WIDTH),
	// 	(int)cap.get(CAP_PROP_FRAME_HEIGHT));

	Size size = Size((int)cap.get(CAP_PROP_FRAME_WIDTH),
		(int)cap.get(CAP_PROP_FRAME_HEIGHT));

	VideoWriter writer;
	double fps = 30.0;
	int fourcc = VideoWriter::fourcc('D', 'I', 'V', 'X');

	FOL_TIME prev_fol = time(NULL);                       // init time
	Create_Folder(prev_fol);                              // init create
	ll init_time = prev_fol / 3600;

	//////////////////////////////////////////////////////// REC START!! ///////////////////////////////////////////////////////////////////////////
	while (1) {

		FOL_TIME end_folder_time = time(NULL);
		// ll folder_hour = (ll)prev_fol + 3600;

		VID_TIME begin_vid_time = time(NULL);
		ll video_minute = (ll)begin_vid_time + 60;

		string video_title = Video_Date_Name();
		writer.open(video_title, fourcc, 30.0, size, true);

		if (!writer.isOpened()) {
			cout << "error!! uu uu so sad " << endl;
			return 1;
		}

		if (init_time < (end_folder_time / 3600)) {

			char new_folder_time_name[100];
			Create_Folder(end_folder_time);
			TimetoString(end_folder_time, new_folder_time_name, BY_MINUTE);
			folder_list.push(new_folder_time_name);
			folder_list.pop();
			init_time = (end_folder_time / 3600);
		}

		if (limit > ava_size) {

			char del_target[100] = "/home/dokyun/DKVScode/Blackbox/";
			char temp[100] = "";

			for (int i = 0; i < folder_list.front().size(); i++) {
				temp[i] = folder_list.front().at(i);
			}
			strcat(del_target, temp);
			Remove_Dir(del_target, 0);
		}

		while (1) {

			VID_TIME end_video_time = time(NULL);
			time_t timer = time(NULL);

			cap.read(img);
			if (img.empty()) {
				cerr << "empty video uu terrible >.<" << endl;
				break;
			}

			writer.write(img);      //동영상 파일에 한 프레임을 저장함.
			imshow("Color", img);
			printf("cur disk size is %lld\n", Get_Disk_Size(DISK_AVA));
			waitKey(15);

			if (end_video_time == video_minute) {
				char new_video_title[100];
				TimetoString(end_video_time, new_video_title, BY_SECOND);
				video_list.pop();
				video_list.push(new_video_title);
				break;
			}
		}
	}

	writer.release();
	cap.release();

	return 0;
}

int main(int argc, char *argv[])
{
	// struct timeval curtime;
	// gettimeofday(&curtime, NULL);
	// printf("%ld: %ld\n", curtime.tv_sec, curtime.tv_usec);

	// DIR* dir;
	// struct dirent *dp;
	// struct statfs stfs;
	// struct stat st;
	// dir = opendir(path);
	// Queue* dir_list = Create_DirQueue();

	Video_Record();
	return 0;
}
