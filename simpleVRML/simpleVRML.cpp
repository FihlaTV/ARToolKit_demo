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

//��������������������������������С����ʼ���������������ͼ��  
static int setupCamera(const char *cparam_name, char *vconf, ARParam *cparam)
{
	ARParam   wparam;//�����������  
	int   xsize, ysize;//����ͼ���С��ֵ  
	//����Ƶ·��  
	if (arVideoOpen(vconf) < 0) {
		fprintf(stderr, "setupCamera(): Unable to open connection to camera.\n");
		return (FALSE);
	}

	//�õ���ǰ��Ƶͼ��Ĵ�С 
	arVideoInqSize(&xsize, &ysize);
	//����������������¶��崰�ڴ�С
	arParamLoad(cparam_name, 1, &wparam);
	arParamChangeSize(&wparam, xsize, ysize, cparam);
	//��ʾ����ֵ
	arParamDisp(cparam);
	//��ʼ���������
	arInitCparam(cparam);
	//��ʼ����ͼ��
	arVideoCapStart();
	return (TRUE);
}
//����һ��������ʶ��ͼ�����  
static int setupMarkersObjects(char *objectDataFilename)
{
	//����һ��������ʶͼ---ѵ����ʶ������ص�λͼ�ļ�������ʹ����object.c�ļ��е�read_VRMLdata()����
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

static void Idle(void) //����һ������������Ӧ�ó����ڿ���ʱִ��ָ���ĺ�����  
{
	static int ms_prev;
	int ms;
	float s_elapsed;
	ARUint8 *image;

	ARMarkerInfo  *marker_info; // Pointer to array holding the details of detected markers.  
	int  marker_num;// Count of number of markers detected.  
	int  i, j, k;

	// Find out how long since Idle() last ran.   
	//�����ϴ�����Idle�������������˶೤ʱ��  
	ms = glutGet(GLUT_ELAPSED_TIME);
	s_elapsed = (float)(ms - ms_prev) * 0.001;
	if (s_elapsed < 0.01f) return; // Don't update more often than 100 Hz.����100HZʱ������  
	ms_prev = ms;

	// Update drawing.  
	arVrmlTimerUpdate();

	//Grab a video frame.����һ֡ͼ��  
	if ((image = arVideoGetImage()) != NULL) {
		gARTImage = image;  // Save the fetched image.������һ֡ͼ��  
		gPatt_found = FALSE;    // Invalidate any previous detected markers.ʹ֮ǰ��⵽���κα�Ƕ���Ч  

		gCallCountMarkerDetect++; // Increment ARToolKit FPS counter.����ARToolKit ֡Ƶ������  

								  // Detect the markers in the video frame.  
								  //��ͼ��֡�м���ʶ  
		if (arDetectMarker(gARTImage, gARTThreshhold, &marker_info, &marker_num) < 0) {
			exit(-1);
		}

		// Check for object visibility.������Ŀ��ӻ�  

		for (i = 0; i < gObjectDataCount; i++) {

			// Check through the marker_info array for highest confidence  
			// visible marker matching our object's pattern.  
			//�Ӽ��������Ʊ�ʶ��������а�ȷ�Ŷ�ƥ���ʶ  
			k = -1;
			for (j = 0; j < marker_num; j++) {
				if (marker_info[j].id == gObjectData[i].id) {
					if (k == -1) k = j; // First marker detected.��⵽�ĵ�һ����ʶ  
					else if (marker_info[k].cf < marker_info[j].cf) k = j; // Higher confidence marker detected.�ҵ���߿��Ŷȵ��Ǹ���ʶ  
				}
			}

			if (k != -1) {
				// Get the transformation between the marker and the real camera.  
				//�õ���ʶ����ʵ����ı任����  
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
		/*����glut��Ҫ��ʾ�������Ѿ��ı���,��ǵ�ǰ������Ҫ���»��ơ�����һ��ѭ���У�
		display���������ص������»��ƴ��ڡ���ʡCPU��Դ����ֹdisplay������û�и��µ�����±�Ƶ�����á�*/
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
// ��������Ҫ���»���ģ�͵�ʱ�򣬵��øú�����
static void Display(void)
{
	int i;
	GLdouble p[16];
	GLdouble m[16];

//��һ���������Ļ����ʾ���µĺ�̨����֡  
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the buffers for new frame.
	arglDispImage(gARTImage, &gARTCparam, 1.0, gArglSettings);	
	arVideoCapNext(); //������һ֡
	gARTImage = NULL; //��������arVideoCapNext()�����󣬴�ʱͼ�����ݲ�����Ч���ȴ�������һ֡ͼ�� 
//�ڶ�����Ѱ�ұ�ʶ�������������������ͶӰ����  
	if (gPatt_found) {
		// ����ҵ���ʶ����ִ������ĳ���
		arglCameraFrustumRH(&gARTCparam, VIEW_DISTANCE_MIN, VIEW_DISTANCE_MAX, p);
		//���������ת����OpenGLͶӰ����16�����飬Ȼ��ֱ�ӵ��룬  
		//���ɵ���ά����ͻ�ƥ����ʵ������ӽ�
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixd(p);
		glMatrixMode(GL_MODELVIEW);
		// Viewing transformation.�Ӿ��任 
		glLoadIdentity();
	
		for (i = 0; i < gObjectDataCount; i++) {//�Ա�ʶ���б���  
			if ((gObjectData[i].visible != 0) && (gObjectData[i].vrml_id >= 0)) {//����ñ�ʶ������Ϊ�ɼ�����������Idle()��������ɣ���ID��ȷ��ô��ִ�����´���  
				arglCameraViewRH(gObjectData[i].trans, m, VIEW_SCALEFACTOR_4);
				/*
				 ����һ���Ӿ����󣬴��ݸ�OpenGL��������������Ӿ��任��
				 ת����ʶת�������OpenGL�Ӿ�������16��ֵ������ʵ�����λ�úͷ���
				 ʹ�������������������λ�ã�
				 ʹ��άĿ��׼ȷ�ķ����������ʶ�ϡ�  
				*/
				glLoadMatrixd(m);//�������������������λ�á�
//���������ڱ�ʶ�ϻ����������塣  
				arVrmlDraw(gObjectData[i].vrml_id);//����ǰ����ص�ģ��ID����ģ��  
			}
		}
	} 

	glutSwapBuffers();//������  
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
