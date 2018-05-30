#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>
#include <AR/config.h>
#include <AR/video.h>
#include <AR/param.h>			
#include <AR/ar.h>
#include <AR/gsub_lite.h>
#include <AR/arvrml.h>
#include "object.h"

// ============================================================================
//	Constants
// ============================================================================

#define VIEW_SCALEFACTOR		0.025		// 1.0 ARToolKit unit becomes 0.025 of my OpenGL units.
#define VIEW_SCALEFACTOR_1		1.0			// 1.0 ARToolKit unit becomes 1.0 of my OpenGL units.
#define VIEW_SCALEFACTOR_4		4.0			// 1.0 ARToolKit unit becomes 4.0 of my OpenGL units.
#define VIEW_DISTANCE_MIN		1.0			// Objects closer to the camera than this will not be displayed.
#define VIEW_DISTANCE_MAX		10000.0		// Objects further away from the camera than this will not be displayed.


static int prefWindowed = TRUE;
static int prefWidth = 640;					// Fullscreen mode width.
static int prefHeight = 480;				// Fullscreen mode height.
static int prefDepth = 32;					// Fullscreen mode bit depth.
static int prefRefresh = 0;					// Fullscreen mode refresh rate. Set to 0 to use default rate.

											// Image acquisition.
static ARUint8		*gARTImage = NULL;
static int			gARTThreshhold = 100;
static long			gCallCountMarkerDetect = 0;
static int			gPatt_found = FALSE;	// At least one marker.

static ARParam		gARTCparam;
static ARGL_CONTEXT_SETTINGS_REF gArglSettings = NULL;

static ObjectData_T	*gObjectData;
static int			gObjectDataCount;

//打开相机，导入相机参数，更改相机长宽大小，初始化相机参数，捕获图像  
static int setupCamera(const char *cparam_name, char *vconf, ARParam *cparam)
{
	ARParam   wparam;//储存相机参数  
	int   xsize, ysize;//储存图像大小的值  
	//打开视频路径  
	if (arVideoOpen(vconf) < 0) {
		fprintf(stderr, "setupCamera(): Unable to open connection to camera.\n");
		return (FALSE);
	}

	//得到当前视频图像的大小 
	arVideoInqSize(&xsize, &ysize);
	//导入相机参数，重新定义窗口大小
	arParamLoad(cparam_name, 1, &wparam);
	arParamChangeSize(&wparam, xsize, ysize, cparam);
	//显示参数值
	arParamDisp(cparam);
	//初始化相机参数
	arInitCparam(cparam);
	//开始捕获图像
	arVideoCapStart();
	return (TRUE);
}
//导入一个或多个标识的图像矩阵  
static int setupMarkersObjects(char *objectDataFilename)
{
	//导入一个或多个标识图---训练标识及其相关的位图文件，这里使用了object.c文件中的read_VRMLdata()函数
	if ((gObjectData = read_VRMLdata(objectDataFilename, &gObjectDataCount)) == NULL) {
		fprintf(stderr, "setupMarkersObjects(): read_VRMLdata returned error !!\n");
		return (FALSE);
	}
	printf("Object count = %d\n", gObjectDataCount);
	return (TRUE);
}

static void Quit(void)
{
	arglCleanup(gArglSettings);
	arVideoCapStop();
	arVideoClose();
	CoUninitialize();
	exit(0);
}

static void Idle(void) //增加一个空闲任务，让应用程序在空闲时执行指定的函数。  
{
	static int ms_prev;
	int ms;
	float s_elapsed;
	ARUint8 *image;

	ARMarkerInfo  *marker_info; // Pointer to array holding the details of detected markers.  
	int  marker_num;// Count of number of markers detected.  
	int  i, j, k;

	// Find out how long since Idle() last ran.   
	//计算上次运行Idle函数到现在用了多长时间  
	ms = glutGet(GLUT_ELAPSED_TIME);
	s_elapsed = (float)(ms - ms_prev) * 0.001;
	if (s_elapsed < 0.01f) return; // Don't update more often than 100 Hz.超过100HZ时不更新  
	ms_prev = ms;

	// Update drawing.  
	arVrmlTimerUpdate();

	//Grab a video frame.捕获一帧图像  
	if ((image = arVideoGetImage()) != NULL) {
		gARTImage = image;  // Save the fetched image.缓存这一帧图像  
		gPatt_found = FALSE;    // Invalidate any previous detected markers.使之前检测到的任何标记都无效  

		gCallCountMarkerDetect++; // Increment ARToolKit FPS counter.增加ARToolKit 帧频计数器  

								  // Detect the markers in the video frame.  
								  //在图像帧中检测标识  
		if (arDetectMarker(gARTImage, gARTThreshhold, &marker_info, &marker_num) < 0) {
			exit(-1);
		}

		// Check for object visibility.检查对象的可视化  

		for (i = 0; i < gObjectDataCount; i++) {

			// Check through the marker_info array for highest confidence  
			// visible marker matching our object's pattern.  
			//从检测出的形似标识物的数组中按确信度匹配标识  
			k = -1;
			for (j = 0; j < marker_num; j++) {
				if (marker_info[j].id == gObjectData[i].id) {
					if (k == -1) k = j; // First marker detected.检测到的第一个标识  
					else if (marker_info[k].cf < marker_info[j].cf) k = j; // Higher confidence marker detected.找到最高可信度的那个标识  
				}
			}

			if (k != -1) {
				// Get the transformation between the marker and the real camera.  
				//得到标识和真实相机的变换矩阵  
				//fprintf(stderr, "Saw object %d.\n", i);  
				if (gObjectData[i].visible == 0) {
					arGetTransMat(&marker_info[k],
						gObjectData[i].marker_center, gObjectData[i].marker_width,
						gObjectData[i].trans);
				}
				else {
					arGetTransMatCont(&marker_info[k], gObjectData[i].trans,
						gObjectData[i].marker_center, gObjectData[i].marker_width,
						gObjectData[i].trans);
				}
				gObjectData[i].visible = 1;
				gPatt_found = TRUE;
			}
			else {
				gObjectData[i].visible = 0;
			}
		}

		// Tell GLUT to update the display.  
		/*告诉glut，要显示的内容已经改变了,标记当前窗口需要重新绘制。在下一次循环中，
		display函数将被回调，重新绘制窗口。节省CPU资源，防止display函数在没有更新的情况下被频繁调用。*/
		glutPostRedisplay();
	}
}
static void Visibility(int visible)
{
	if (visible == GLUT_VISIBLE) {
		glutIdleFunc(Idle);
	}
	else {
		glutIdleFunc(NULL);
	}
}

static void Reshape(int w, int h)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

}
// 当窗口需要重新绘制模型的时候，调用该函数。
static void Display(void)
{
	int i;
	GLdouble p[16];
	GLdouble m[16];

//第一步：清除屏幕，显示最新的后台缓冲帧  
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the buffers for new frame.
	arglDispImage(gARTImage, &gARTCparam, 1.0, gArglSettings);	
	arVideoCapNext(); //开启下一帧
	gARTImage = NULL; //当调用了arVideoCapNext()函数后，此时图像数据不再有效，等待接收下一帧图像。 
//第二步：寻找标识，并根据相机参数设置投影矩阵  
	if (gPatt_found) {
		// 如果找到标识，就执行下面的程序
		arglCameraFrustumRH(&gARTCparam, VIEW_DISTANCE_MIN, VIEW_DISTANCE_MAX, p);
		//把相机参数转换成OpenGL投影矩阵16的数组，然后直接导入，  
		//生成的三维物体就会匹配真实的相机视角
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixd(p);
		glMatrixMode(GL_MODELVIEW);
		// Viewing transformation.视景变换 
		glLoadIdentity();
	
		for (i = 0; i < gObjectDataCount; i++) {//对标识进行遍历  
			if ((gObjectData[i].visible != 0) && (gObjectData[i].vrml_id >= 0)) {//如果该标识被设置为可见（该设置在Idle()函数中完成）且ID正确那么就执行以下代码  
				arglCameraViewRH(gObjectData[i].trans, m, VIEW_SCALEFACTOR_4);
				/*
				 创建一个视景矩阵，传递给OpenGL设置虚拟相机的视景变换。
				 转换标识转换矩阵成OpenGL视景矩阵，这16个值就是真实相机的位置和方向，
				 使用他们设置虚拟相机的位置，
				 使三维目标准确的放置在物理标识上。  
				*/
				glLoadMatrixd(m);//这里是设置虚拟相机的位置。
//第三步：在标识上画出虚拟物体。  
				arVrmlDraw(gObjectData[i].vrml_id);//根据前面加载的模型ID绘制模型  
			}
		}
	} 

	glutSwapBuffers();//清理缓存  
}

int main(int argc, char** argv)
{
	int i;
	char glutGamemode[32];
	const char *cparam_name ="../Data/camera_para.dat";
	char			*vconf = "../Data/WDM_camera_flipV.xml";
	char objectDataFilename[] = "../Data/object_data_vrml";
	glutInit(&argc, argv);
	if (!setupCamera(cparam_name, vconf, &gARTCparam)) {
		fprintf(stderr, "main(): Unable to set up AR camera.\n");
		exit(-1);
	}
	CoInitialize(NULL);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	if (!prefWindowed) {
		if (prefRefresh) sprintf(glutGamemode, "%ix%i:%i@%i", prefWidth, prefHeight, prefDepth, prefRefresh);
		else sprintf(glutGamemode, "%ix%i:%i", prefWidth, prefHeight, prefDepth);
		glutGameModeString(glutGamemode);
		glutEnterGameMode();
	}
	else {
		glutInitWindowSize(gARTCparam.xsize, gARTCparam.ysize);
		glutCreateWindow(argv[0]);
	}
	if ((gArglSettings = arglSetupForCurrentContext()) == NULL) {
		fprintf(stderr, "main(): arglSetupForCurrentContext() returned error.\n");
		exit(-1);
	}
	arUtilTimerReset();

	if (!setupMarkersObjects(objectDataFilename)) {
		fprintf(stderr, "main(): Unable to set up AR objects and markers.\n");
		Quit();
	}

	// Test render all the VRML objects.
	fprintf(stdout, "Pre-rendering the VRML objects...");
	fflush(stdout);
	glEnable(GL_TEXTURE_2D);
	for (i = 0; i < gObjectDataCount; i++) {
		arVrmlDraw(gObjectData[i].vrml_id);
	}
	glDisable(GL_TEXTURE_2D);
	fprintf(stdout, " done\n");

	// Register GLUT event-handling callbacks.
	// NB: Idle() is registered by Visibility.
	glutDisplayFunc(Display);
	glutReshapeFunc(Reshape);
	glutVisibilityFunc(Visibility);

	glutMainLoop();
	system("pause");
	return (0);
}
