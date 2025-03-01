#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <typeinfo>

#include "OpenGLExampleBrowser.h"
#include "LinearMath/btQuickprof.h"
#include "LinearMath/btThreads.h"
#include "LinearMath/btIDebugDraw.h"
#include "LinearMath/btSerializer.h"
#include "Bullet3Common/b3Vector3.h"
#include "Bullet3Common/b3CommandLineArgs.h"
#include "Bullet3Common/b3FileUtils.h"
#include "Bullet3Common/b3HashMap.h"
#include "../OpenGLWindow/OpenGLInclude.h"
#include "../OpenGLWindow/SimpleOpenGL2App.h"
#include "../OpenGLWindow/SimpleCamera.h"
#include "../OpenGLWindow/SimpleOpenGL2Renderer.h"

#ifndef NO_OPENGL3
#include "../OpenGLWindow/SimpleOpenGL3App.h"
#endif

#include "../CommonInterfaces/CommonRenderInterface.h"
#include "../CommonInterfaces/Common2dCanvasInterface.h"
#include "../CommonInterfaces/CommonExampleInterface.h"

#ifdef __APPLE__
#include "../OpenGLWindow/MacOpenGLWindow.h"
#elif defined(_WIN32)
#include "../OpenGLWindow/Win32OpenGLWindow.h"
#elif defined(BT_USE_EGL)
#include "../OpenGLWindow/EGLOpenGLWindow.h"
#else
#include "../OpenGLWindow/X11OpenGLWindow.h"
#endif

#include "../ThirdPartyLibs/Gwen/Renderers/OpenGL_DebugFont.h"
#include "../Utils/b3Clock.h"
#include "../Utils/ChromeTraceUtil.h"
#include "GwenGUISupport/gwenInternalData.h"
#include "GwenGUISupport/gwenUserInterface.h"
#include "GwenGUISupport/GwenParameterInterface.h"

#ifndef BT_NO_PROFILE
#include "GwenGUISupport/GwenProfileWindow.h"
#endif

#include "GwenGUISupport/GwenTextureWindow.h"
#include "GwenGUISupport/GraphingTexture.h"

//quick test for file import, @todo(erwincoumans) make it more general and add other file formats
#include "../Importers/ImportURDFDemo/ImportURDFSetup.h"
#include "../Importers/ImportBullet/SerializeSetup.h"

#include "../SharedMemory/SharedMemoryPublic.h"

#include "../Extras/Serialize/BulletWorldImporter/btMultiBodyWorldImporter.h"
#include "../Extras/Serialize/BulletFileLoader/btBulletFile.h"
#include "ExampleEntries.h"
#include "OpenGLGuiHelper.h"
#include "EmptyExample.h"

struct GL3TexLoader : public MyTextureLoader
{
	b3HashMap<b3HashString, GLint> m_hashMap;

	virtual void LoadTexture(Gwen::Texture* pTexture)
	{
		Gwen::String namestr = pTexture->name.Get();
		const char* n = namestr.c_str();
		GLint* texIdPtr = m_hashMap[n];
		if (texIdPtr)
		{
			pTexture->m_intData = *texIdPtr;
		}
	}
	virtual void FreeTexture(Gwen::Texture* pTexture)
	{
	}
};

struct OpenGLExampleBrowserInternalData
{
	Gwen::Renderer::Base* m_gwenRenderer;
	CommonGraphicsApp* m_app;
#ifndef BT_NO_PROFILE
	MyProfileWindow* m_profWindow;
#endif  //BT_NO_PROFILE
	btAlignedObjectArray<Gwen::Controls::TreeNode*> m_nodes;
	GwenUserInterface* m_gui;
	GL3TexLoader* m_myTexLoader;
	struct MyMenuItemHander* m_handler2;
	btAlignedObjectArray<MyMenuItemHander*> m_handlers;

	/* Kaedenn 2019/10/27 */
	bool m_verboseMode;

	OpenGLExampleBrowserInternalData()
		: m_gwenRenderer(NULL),
		  m_app(NULL),
#ifndef BT_NO_PROFILE
		  m_profWindow(NULL),
#endif
		  m_gui(NULL),
		  m_myTexLoader(NULL),
		  m_handler2(NULL),
		  m_verboseMode(false)
	{
	}
};

static CommonGraphicsApp* s_app = NULL;

static CommonWindowInterface* s_window = NULL;
static CommonParameterInterface* s_parameterInterface = NULL;
static CommonRenderInterface* s_instancingRenderer = NULL;
static OpenGLGuiHelper* s_guiHelper = NULL;
#ifndef BT_NO_PROFILE
static MyProfileWindow* s_profWindow = NULL;
#endif  //BT_NO_PROFILE
static SharedMemoryInterface* sSharedMem = NULL;

#define DEMO_SELECTION_COMBOBOX 13

static const char* startFileName = "0_Bullet3Demo.txt";
static const char* startSaveFileName = "0_Bullet3Demo.bullet";
static char saveFileName[1024] = {0};
static char staticPngFileName[1024] = {0};
//static GwenUserInterface* gui = NULL;
static GwenUserInterface* gui2 = NULL;
static int sCurrentDemoIndex = -1;
static int sCurrentHightlighted = 0;
static CommonExampleInterface* sCurrentDemo = NULL;
static b3AlignedObjectArray<const char*> allNames;
static float gFixedTimeStep = 0;
static bool gAllowRetina = true;
static bool gDisableDemoSelection = false;
static int gRenderDevice = -1;
static int gWindowBackend = 0;
static ExampleEntries* gAllExamples = NULL;
static bool sUseOpenGL2 = false;

#ifndef USE_OPENGL3
extern bool useShadowMap;
#endif

static bool visualWireframe = false;
static bool renderVisualGeometry = true;
static bool renderGrid = true;
static bool gEnableRenderLoop = true;

static bool renderGui = true;
static bool enable_experimental_opencl = false;

static bool gEnableDefaultKeyboardShortcuts = true;
static bool gEnableDefaultMousePicking = true;

static int gDebugDrawFlags = 0;
static bool pauseSimulation = false;
static bool singleStepSimulation = false;
static int midiBaseIndex = 176;
extern bool gDisableDeactivation;

/* Used by other modules */
int gSharedMemoryKey = -1;

///some quick test variable for the OpenCL examples

static int gPreferredOpenCLDeviceIndex = -1;
static int gPreferredOpenCLPlatformIndex = -1;
static int gGpuArraySizeX = 45;
static int gGpuArraySizeY = 55;
static int gGpuArraySizeZ = 45;

/* Kaedenn 2019/10/27: backtrace support */
#include <string>
#include <vector>
#include <unistd.h>
#include <execinfo.h>
static int gStackTrimStart = 0;
static int gStackTrimEnd = 0;
static std::vector<std::string> gStackTrimFilters;
static void PrintStackTrace();

/* Kaedenn 2019/10/27: modifier keys */
enum
{
	MOD_ALT = 1,
	MOD_SHIFT = 2,
	MOD_CONTROL = 4
};

//#include <float.h>
//unsigned int fp_control_state = _controlfp(_EM_INEXACT, _MCW_EM);

void deleteDemo()
{
	if (sCurrentDemo)
	{
		sCurrentDemo->exitPhysics();
		s_instancingRenderer->removeAllInstances();
		delete sCurrentDemo;
		sCurrentDemo = 0;
		delete s_guiHelper;
		s_guiHelper = 0;

		//	  CProfileManager::CleanupMemory();
	}
}

char* gPngFilePrefix = NULL;
const char* gPngFileName = NULL;
int gPngSkipFrames = 0;

b3KeyboardCallback prevKeyboardCallback = 0;
void MyKeyboardCallback(int key, int state)
{
	//b3Printf("key=%d, state=%d", key, state);
	bool handled = false;
	if (renderGui)
	{
		if (gui2 && !handled)
		{
			handled = gui2->keyboardCallback(key, state);
		}
	}

	if (!handled && sCurrentDemo)
	{
		handled = sCurrentDemo->keyboardCallback(key, state);
	}

	/* Kaedenn 2019/10/27: extract modifier information from state */
	bool isPressed = ((state & 1) != 0);
	bool hasAlt = (((state >> 1) & MOD_ALT) != 0);
	bool hasShift = (((state >> 1) & MOD_SHIFT) != 0);
	bool hasCtrl = (((state >> 1) & MOD_CONTROL) != 0);
	bool hasAnyMod = (hasAlt || hasShift || hasCtrl);

	//checkout: is it desired to ignore keys, if the demo already handles them?
	//if (handled)
	//  return;

	if (gEnableDefaultKeyboardShortcuts)
	{
		if (key == 'a' && isPressed)
		{
			gDebugDrawFlags ^= btIDebugDraw::DBG_DrawAabb;
			b3Printf("Toggling %s", "DBG_DrawAabb");
		}
		if (key == 'c' && isPressed)
		{
			gDebugDrawFlags ^= btIDebugDraw::DBG_DrawContactPoints;
			b3Printf("Toggling %s", "DBG_DrawContactPoints");
		}
		if (key == 'd' && isPressed)
		{
			gDebugDrawFlags ^= btIDebugDraw::DBG_NoDeactivation;
			gDisableDeactivation = ((gDebugDrawFlags & btIDebugDraw::DBG_NoDeactivation) != 0);
			b3Printf("Toggling %s", "DBG_NoDeactivation");
		}
		if (key == 'j' && isPressed)
		{
			gDebugDrawFlags ^= btIDebugDraw::DBG_DrawFrames;
			b3Printf("Toggling %s", "DBG_DrawFrames");
		}

		if (key == 'k' && isPressed)
		{
			gDebugDrawFlags ^= btIDebugDraw::DBG_DrawConstraints;
			b3Printf("Toggling %s", "DBG_DrawConstraints");
		}

		if (key == 'l' && isPressed)
		{
			gDebugDrawFlags ^= btIDebugDraw::DBG_DrawConstraintLimits;
			b3Printf("Toggling %s", "DBG_DrawConstraintLimits");
		}
		if (key == 'w' && isPressed)
		{
			visualWireframe = !visualWireframe;
			gDebugDrawFlags ^= btIDebugDraw::DBG_DrawWireframe;
			b3Printf("Toggling %s", "DBG_DrawWireframe");
		}

		if (key == 'v' && isPressed)
		{
			renderVisualGeometry = !renderVisualGeometry;
			b3Printf("Toggling %s", "renderVisualGeometry");
		}
		if (key == 'g' && isPressed)
		{
			renderGrid = !renderGrid;
			renderGui = !renderGui;
			b3Printf("Toggling %s", "renderGrid and renderGui");
		}

		if (key == 'i' && isPressed)
		{
			pauseSimulation = !pauseSimulation;
			b3Printf("Toggling %s", "pauseSimulation");
		}
		if (key == 'o' && isPressed)
		{
			singleStepSimulation = true;
			b3Printf("Setting %s", "singleStepSimulation");
		}

		if (key == 'p')
		{
			if (isPressed)
			{
				b3ChromeUtilsStartTimings();
			}
			else
			{
#ifdef _WIN32
				b3ChromeUtilsStopTimingsAndWriteJsonFile("timings");
				b3Printf("Logged timings to %s", "timings");
#else
				b3ChromeUtilsStopTimingsAndWriteJsonFile("/tmp/timings");
				b3Printf("Logged timings to %s", "/tmp/timings");
#endif
			}
		}

#ifndef NO_OPENGL3
		if (key == 's' && isPressed)
		{
			useShadowMap = !useShadowMap;
			b3Printf("Toggling %s", "useShadowMap");
		}
#endif
		if (key == B3G_F1)
		{
			static int count = 0;
			if (isPressed)
			{
				b3Printf("F1 pressed %d", count++);

				if (gPngFileName)
				{
					b3Printf("disable image dump");

					gPngFileName = 0;
				}
				else
				{
					gPngFileName = gAllExamples->getExampleName(sCurrentDemoIndex);
					b3Printf("enable image dump %s", gPngFileName);
				}
			}
			else
			{
				b3Printf("F1 released %d", count++);
			}
		}
	}
	if (key == B3G_ESCAPE && s_window)
	{
		s_window->setRequestExit();
	}

	if (prevKeyboardCallback)
		prevKeyboardCallback(key, state);
}

b3MouseMoveCallback prevMouseMoveCallback = 0;
static void MyMouseMoveCallback(float x, float y)
{
	bool handled = false;
	if (sCurrentDemo)
		handled = sCurrentDemo->mouseMoveCallback(x, y);
	if (renderGui)
	{
		if (!handled && gui2)
			handled = gui2->mouseMoveCallback(x, y);
	}
	if (!handled)
	{
		if (prevMouseMoveCallback)
			prevMouseMoveCallback(x, y);
	}
}

b3MouseButtonCallback prevMouseButtonCallback = 0;
static void MyMouseButtonCallback(int button, int state, float x, float y)
{
	bool handled = false;
	//try picking first
	if (sCurrentDemo)
		handled = sCurrentDemo->mouseButtonCallback(button, state, x, y);

	if (renderGui)
	{
		if (!handled && gui2)
			handled = gui2->mouseButtonCallback(button, state, x, y);
	}
	if (!handled)
	{
		if (prevMouseButtonCallback)
			prevMouseButtonCallback(button, state, x, y);
	}
	//  b3DefaultMouseButtonCallback(button,state,x,y);
}

struct FileImporterByExtension
{
	std::string m_extension;
	CommonExampleInterface::CreateFunc* m_createFunc;
};

static btAlignedObjectArray<FileImporterByExtension> gFileImporterByExtension;

void OpenGLExampleBrowser::registerFileImporter(const char* extension, CommonExampleInterface::CreateFunc* createFunc)
{
	FileImporterByExtension fi;
	fi.m_extension = extension;
	fi.m_createFunc = createFunc;
	gFileImporterByExtension.push_back(fi);
}

void OpenGLExampleBrowserVisualizerFlagCallback(int flag, bool enable)
{
	if (flag == COV_ENABLE_Y_AXIS_UP)
	{
		//either Y = up or Z
		int upAxis = enable ? 1 : 2;
		s_app->setUpAxis(upAxis);
	}

	if (flag == COV_ENABLE_RENDERING)
	{
		gEnableRenderLoop = (enable != 0);
	}

	if (flag == COV_ENABLE_SINGLE_STEP_RENDERING)
	{
		if (enable)
		{
			gEnableRenderLoop = false;
			singleStepSimulation = true;
		}
		else
		{
			gEnableRenderLoop = true;
			singleStepSimulation = false;
		}
	}

	if (flag == COV_ENABLE_SHADOWS)
	{
		useShadowMap = enable;
	}
	if (flag == COV_ENABLE_GUI)
	{
		renderGui = enable;
		renderGrid = enable;
	}

	if (flag == COV_ENABLE_KEYBOARD_SHORTCUTS)
	{
		gEnableDefaultKeyboardShortcuts = enable;
	}
	if (flag == COV_ENABLE_MOUSE_PICKING)
	{
		gEnableDefaultMousePicking = enable;
	}

	if (flag == COV_ENABLE_WIREFRAME)
	{
		visualWireframe = enable;
		if (visualWireframe)
		{
			gDebugDrawFlags |= btIDebugDraw::DBG_DrawWireframe;
		}
		else
		{
			gDebugDrawFlags &= ~btIDebugDraw::DBG_DrawWireframe;
		}
	}
}

void openFileDemo(const char* filename)
{
	deleteDemo();

	s_guiHelper = new OpenGLGuiHelper(s_app, sUseOpenGL2);
	s_guiHelper->setVisualizerFlagCallback(OpenGLExampleBrowserVisualizerFlagCallback);

	s_parameterInterface->removeAllParameters();

	CommonExampleOptions options(s_guiHelper, 1);
	options.m_fileName = filename;
	char fullPath[1024];
	sprintf(fullPath, "%s", filename);
	b3FileUtils::toLower(fullPath);

	for (int i = 0; i < gFileImporterByExtension.size(); i++)
	{
		if (strstr(fullPath, gFileImporterByExtension[i].m_extension.c_str()))
		{
			sCurrentDemo = gFileImporterByExtension[i].m_createFunc(options);
		}
	}

	if (sCurrentDemo)
	{
		sCurrentDemo->initPhysics();
		sCurrentDemo->resetCamera();
	}
}

void selectDemo(int demoIndex)
{
	bool resetCamera = (sCurrentDemoIndex != demoIndex);
	sCurrentDemoIndex = demoIndex;
	sCurrentHightlighted = demoIndex;
	int numDemos = gAllExamples->getNumRegisteredExamples();

	if (demoIndex > numDemos)
	{
		demoIndex = 0;
	}
	deleteDemo();

	CommonExampleInterface::CreateFunc* func = gAllExamples->getExampleCreateFunc(demoIndex);
	if (func)
	{
		if (s_parameterInterface)
		{
			s_parameterInterface->removeAllParameters();
		}
		int option = gAllExamples->getExampleOption(demoIndex);
		s_guiHelper = new OpenGLGuiHelper(s_app, sUseOpenGL2);
		s_guiHelper->setVisualizerFlagCallback(OpenGLExampleBrowserVisualizerFlagCallback);

		CommonExampleOptions options(s_guiHelper, option);
		options.m_sharedMem = sSharedMem;
		sCurrentDemo = (*func)(options);
		if (sCurrentDemo)
		{
			if (gui2)
			{
				gui2->setStatusBarMessage("Status: OK", false);
			}
			/*b3Printf("Selected demo: %s", gAllExamples->getExampleName(demoIndex));*/
			if (gui2)
			{
				gui2->setExampleDescription(gAllExamples->getExampleDescription(demoIndex));
			}

			sCurrentDemo->initPhysics();
			if (resetCamera)
			{
				sCurrentDemo->resetCamera();
			}
		}
	}
}

static void saveCurrentSettings(int currentEntry, const char* startFileName)
{
	FILE* f = fopen(startFileName, "w");
	if (f)
	{
		fprintf(f, "--start_demo_name=%s\n", gAllExamples->getExampleName(sCurrentDemoIndex));
		fprintf(f, "--mouse_move_multiplier=%f\n", s_app->getMouseMoveMultiplier());
		fprintf(f, "--mouse_wheel_multiplier=%f\n", s_app->getMouseWheelMultiplier());
		float red, green, blue;
		s_app->getBackgroundColor(&red, &green, &blue);
		fprintf(f, "--background_color_red= %f\n", red);
		fprintf(f, "--background_color_green= %f\n", green);
		fprintf(f, "--background_color_blue= %f\n", blue);
		fprintf(f, "--fixed_timestep= %f\n", gFixedTimeStep);
		if (!gAllowRetina)
		{
			fprintf(f, "--disable_retina");
		}

		if (enable_experimental_opencl)
		{
			fprintf(f, "--enable_experimental_opencl\n");
		}
		//	  if (sUseOpenGL2 )
		//	  {
		//		  fprintf(f,"--opengl2\n");
		//	  }

		fclose(f);
	}
};

static void loadCurrentSettings(const char* startFileName, b3CommandLineArgs& args)
{
	//int currentEntry= 0;
	FILE* f = fopen(startFileName, "r");
	if (f)
	{
		char oneline[1024] = {0};
		char* argv[] = {0, &oneline[0]};

		while (fgets(oneline, 1024, f) != NULL)
		{
			char* pos;
			if ((pos = strchr(oneline, '\n')) != NULL)
				*pos = '\0';
			args.addArgs(2, argv);
		}
		fclose(f);
	}
};

void MyComboBoxCallback(int comboId, const char* item)
{
	//printf("comboId = %d, item = %s\n",comboId, item);
	if (comboId == DEMO_SELECTION_COMBOBOX)
	{
		//find selected item
		for (int i = 0; i < allNames.size(); i++)
		{
			if (strcmp(item, allNames[i]) == 0)
			{
				selectDemo(i);
				saveCurrentSettings(sCurrentDemoIndex, startFileName);
				break;
			}
		}
	}
}

//in case of multi-threading, don't submit messages while the GUI is rendering (causing crashes)
static bool gBlockGuiMessages = false;

/* Kaedenn: 2019/10/27 */
static void PrintStackTrace()
{
	/* Requires glibc >= 2.1 */
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#if (__GNUC__ << 16) + __GNUC_MINOR__ >= (2 << 16) + 1
	void* buffer[512] = {0};
	char** strings = NULL;
	int nptrs = backtrace(buffer, sizeof(buffer)/sizeof(void*));
	if ((strings = backtrace_symbols(buffer, nptrs)) != NULL)
	{
		fprintf(stderr, "Backtrace:\n");
		for (int i = gStackTrimStart; i < nptrs - gStackTrimStart - gStackTrimEnd; ++i)
		{
			/* Check for filtering against gStackTrimFilters */
			bool filter = false;
			for (int j = 0; j < gStackTrimFilters.size(); ++j)
			{
				if (strstr(strings[i], gStackTrimFilters[j].c_str()) != NULL)
				{
					/* Filter is present; line is filtered out */
					filter = true;
					break;
				}
			}
			if (!filter)
			{
				fprintf(stderr, "\tat %d %s\n", i, strings[i]);
			}
		}
		fflush(stderr);
		free(strings);
	}
	else
	{
		fprintf(stderr, "Calling backtrace_symbols(%p, %lu): error %d: %s\n",
				buffer, sizeof(buffer)/sizeof(void*),
				errno, strerror(errno));
	}
#endif
#endif
}

void MyGuiPrintf(const char* msg)
{
	/* Kaedenn 2019/10/27: Print "\n" only if msg doeesn't contain one */
	printf("b3Printf: %s", msg);
	if (strchr(msg, '\n') == NULL)
	{
		printf("\n");
	}
	if (!gDisableDemoSelection && !gBlockGuiMessages)
	{
		gui2->textOutput(msg);
		gui2->forceUpdateScrollBars();
	}
}

void MyStatusBarPrintf(const char* msg)
{
	/* Kaedenn 2019/10/27: Print "\n" only if msg doeesn't contain one */
	printf("b3Printf: %s", msg);
	if (strchr(msg, '\n') == NULL)
	{
		printf("\n");
	}
	if (!gDisableDemoSelection && !gBlockGuiMessages)
	{
		bool isLeft = true;
		gui2->setStatusBarMessage(msg, isLeft);
	}
}

void MyStatusBarError(const char* msg)
{
	/* Kaedenn 2019/11/02: Print "\n" only if msg doesn't contain one */
	printf("b3Warning: %s", msg);
	if (strchr(msg, '\n') == NULL)
	{
		printf("\n");
	}
	PrintStackTrace();
	if (!gDisableDemoSelection && !gBlockGuiMessages)
	{
		bool isLeft = false;
		gui2->setStatusBarMessage(msg, isLeft);
		gui2->textOutput(msg);
		gui2->forceUpdateScrollBars();
	}
	btAssert(0);
}

struct MyMenuItemHander : public Gwen::Event::Handler
{
	int m_buttonId;

	MyMenuItemHander(int buttonId)
		: m_buttonId(buttonId)
	{
	}

	void onButtonA(Gwen::Controls::Base* pControl)
	{
		//const Gwen::String& name = pControl->GetName();
		Gwen::Controls::TreeNode* node = (Gwen::Controls::TreeNode*)pControl;
		//  Gwen::Controls::Label* l = node->GetButton();

		Gwen::UnicodeString la = node->GetButton()->GetText();  // node->GetButton()->GetName();// GetText();
		Gwen::String laa = Gwen::Utility::UnicodeToString(la);
		//  const char* ha = laa.c_str();

		//printf("selected %s\n", ha);
		//int dep = but->IsDepressed();
		//int tog = but->GetToggleState();
		//	  if (m_data->m_toggleButtonCallback)
		//	  (*m_data->m_toggleButtonCallback)(m_buttonId, tog);
	}
	void onButtonB(Gwen::Controls::Base* pControl)
	{
		Gwen::Controls::Label* label = (Gwen::Controls::Label*)pControl;
		Gwen::UnicodeString la = label->GetText();  // node->GetButton()->GetName();// GetText();
		Gwen::String laa = Gwen::Utility::UnicodeToString(la);
		//const char* ha = laa.c_str();

		if (!gDisableDemoSelection)
		{
			selectDemo(sCurrentHightlighted);
			saveCurrentSettings(sCurrentDemoIndex, startFileName);
		}
	}
	void onButtonC(Gwen::Controls::Base* pControl)
	{
		/*Gwen::Controls::Label* label = (Gwen::Controls::Label*) pControl;
		Gwen::UnicodeString la = label->GetText();// node->GetButton()->GetName();// GetText();
		Gwen::String laa = Gwen::Utility::UnicodeToString(la);
		const char* ha = laa.c_str();


		printf("onButtonC ! %s\n", ha);
		*/
	}
	void onButtonD(Gwen::Controls::Base* pControl)
	{
		/*	  Gwen::Controls::Label* label = (Gwen::Controls::Label*) pControl;
		Gwen::UnicodeString la = label->GetText();// node->GetButton()->GetName();// GetText();
		Gwen::String laa = Gwen::Utility::UnicodeToString(la);
		const char* ha = laa.c_str();
		*/

		//  printf("onKeyReturn ! \n");
		if (!gDisableDemoSelection)
		{
			selectDemo(sCurrentHightlighted);
			saveCurrentSettings(sCurrentDemoIndex, startFileName);
		}
	}

	void onButtonE(Gwen::Controls::Base* pControl)
	{
		//  printf("select %d\n",m_buttonId);
		sCurrentHightlighted = m_buttonId;
		gui2->setExampleDescription(gAllExamples->getExampleDescription(sCurrentHightlighted));
	}

	void onButtonF(Gwen::Controls::Base* pControl)
	{
		//printf("selection changed!\n");
	}

	void onButtonG(Gwen::Controls::Base* pControl)
	{
		//printf("onButtonG !\n");
	}
};

void quitCallback()
{
	s_window->setRequestExit();
}

/* Kaedenn 2019/09/29 */
void saveCallback()
{
	const char* filePath = saveFileName;
	if (!filePath || !filePath[0]) {
		filePath = startSaveFileName;
	}
	b3Printf("Called saveCallback(%s)", filePath);
	FILE* f = fopen(filePath, "wb");
	if (f) {
		btDefaultSerializer* ser = new btDefaultSerializer();
		int currentFlags = ser->getSerializationFlags();
		ser->setSerializationFlags(currentFlags | BT_SERIALIZE_CONTACT_MANIFOLDS);
		/* TODO:
		 * Send a request to the server to send back a serialized
		 * b3DynamicsWorld
		 */
		fwrite(ser->getBufferPointer(), ser->getCurrentBufferSize(), 1, f);
		delete ser;
		fclose(f);
	} else {
		int errnr = errno;
		b3Error("Failed fopen(%s, \"wb\"): %d: %s", filePath, errnr, strerror(errnr));
	}
}

void fileOpenCallback()
{
	char filename[1024];
	int len = s_window->fileOpenDialog(filename, 1024);
	if (len)
	{
		//todo(erwincoumans) check if it is actually URDF
		//printf("file open:%s\n", filename);
		openFileDemo(filename);
	}
}

#define MAX_GRAPH_WINDOWS 5

struct QuickCanvas : public Common2dCanvasInterface
{
	GL3TexLoader* m_myTexLoader;

	MyGraphWindow* m_gw[MAX_GRAPH_WINDOWS];
	GraphingTexture* m_gt[MAX_GRAPH_WINDOWS];
	int m_curNumGraphWindows;

	QuickCanvas(GL3TexLoader* myTexLoader)
		: m_myTexLoader(myTexLoader),
		  m_curNumGraphWindows(0)
	{
		for (int i = 0; i < MAX_GRAPH_WINDOWS; i++)
		{
			m_gw[i] = 0;
			m_gt[i] = 0;
		}
	}
	virtual ~QuickCanvas() {}
	virtual int createCanvas(const char* canvasName, int width, int height, int xPos, int yPos)
	{
		if (m_curNumGraphWindows < MAX_GRAPH_WINDOWS)
		{
			//find a slot
			int slot = m_curNumGraphWindows;
			btAssert(slot < MAX_GRAPH_WINDOWS);
			if (slot >= MAX_GRAPH_WINDOWS)
				return 0;  //don't crash

			m_curNumGraphWindows++;

			MyGraphInput input(gui2->getInternalData());
			input.m_width = width;
			input.m_height = height;
			input.m_xPos = xPos;
			input.m_yPos = yPos;
			input.m_name = canvasName;
			input.m_texName = canvasName;
			m_gt[slot] = new GraphingTexture;
			m_gt[slot]->create(width, height);
			int texId = m_gt[slot]->getTextureId();
			m_myTexLoader->m_hashMap.insert(canvasName, texId);
			m_gw[slot] = setupTextureWindow(input);

			return slot;
		}
		return -1;
	}
	virtual void destroyCanvas(int canvasId)
	{
		btAssert(canvasId >= 0);
		delete m_gt[canvasId];
		m_gt[canvasId] = 0;
		destroyTextureWindow(m_gw[canvasId]);
		m_gw[canvasId] = 0;
		m_curNumGraphWindows--;
	}
	virtual void setPixel(int canvasId, int x, int y, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
	{
		btAssert(canvasId >= 0);
		btAssert(canvasId < m_curNumGraphWindows);
		m_gt[canvasId]->setPixel(x, y, red, green, blue, alpha);
	}

	virtual void getPixel(int canvasId, int x, int y, unsigned char& red, unsigned char& green, unsigned char& blue, unsigned char& alpha)
	{
		btAssert(canvasId >= 0);
		btAssert(canvasId < m_curNumGraphWindows);
		m_gt[canvasId]->getPixel(x, y, red, green, blue, alpha);
	}

	virtual void refreshImageData(int canvasId)
	{
		m_gt[canvasId]->uploadImageData();
	}
};

OpenGLExampleBrowser::OpenGLExampleBrowser(class ExampleEntries* examples)
{
	m_internalData = new OpenGLExampleBrowserInternalData;

	gAllExamples = examples;
}

OpenGLExampleBrowser::~OpenGLExampleBrowser()
{
	deleteDemo();
	for (int i = 0; i < m_internalData->m_nodes.size(); i++)
	{
		delete m_internalData->m_nodes[i];
	}
	delete m_internalData->m_handler2;
	for (int i = 0; i < m_internalData->m_handlers.size(); i++)
	{
		delete m_internalData->m_handlers[i];
	}
	m_internalData->m_handlers.clear();
	m_internalData->m_nodes.clear();
	delete s_parameterInterface;
	s_parameterInterface = 0;
	delete s_app->m_2dCanvasInterface;
	s_app->m_2dCanvasInterface = 0;

#ifndef BT_NO_PROFILE
	destroyProfileWindow(m_internalData->m_profWindow);
#endif

	m_internalData->m_gui->exit();

	delete m_internalData->m_gui;
	delete m_internalData->m_gwenRenderer;
	delete m_internalData->m_myTexLoader;

	delete m_internalData->m_app;
	s_app = 0;

	delete m_internalData;

	gFileImporterByExtension.clear();
	gAllExamples = 0;
}

bool OpenGLExampleBrowser::init(int argc, char* argv[])
{
	b3CommandLineArgs args(argc, argv);

	loadCurrentSettings(startFileName, args);

	b3SetCustomWarningMessageFunc(MyGuiPrintf);
	b3SetCustomPrintfFunc(MyGuiPrintf);
	b3SetCustomErrorMessageFunc(MyStatusBarError);

	/* Kaedenn 2019/10/27 */
	if (args.CheckCmdLineFlag("help"))
	{
		fprintf(stderr, "OpenGLExampleBrowser usage:\n"
"  --background_color_blue=VAL  background color blue component (0..1)\n"
"  --background_color_green=VAL background color green component (0..1)\n"
"  --background_color_red=VAL   background color red component (0..1)\n"
"  --disable_retina             disallow retina display\n"
"  --enable_experimental_opencl enable experimental OpenCL examples\n"
"  --hide_explorer              hide the left Explorer window in the GUI\n"
"  --nogui                      start with the GUI hidden\n"
"  --opengl2                    use OpenGL2 fallback over OpenGL3\n"
"  --paused                     start with the simulation paused\n"
"  --tracing                    enable tracing\n"
"  --verbose                    enable verbose output\n"
"  --width=PIXELS               width of the example browser window\n"
"  --height=PIXELS              height of the example browser window\n"
"  --fixed_timestep=SEC         set a custom fixed timestep\n"
"  --mouse_move_multiplier=N    mouse movement acceleration multiplier\n"
"  --mouse_wheel_multiplier=N   mouse wheel acceleration multiplier\n"
"  --mp4=PATH                   dump simulation to a video file\n"
"  --png_prefix=STR             prefix directory/name for generated PNGs\n"
"  --png_skip_frames=NUM        frames to skip when generating PNGs\n"
"  --render_device=ARG          OpenGL2 rendering target if OpenGL3 is unsupported\n"
"  --save_bullet=PATH           save simulation to a .bullet file\n"
"  --shared_memory_key=KEY      use a specific shared memory key\n"
"  --stack_trim=PAT             stack trim words separated by a semicolon\n"
"  --stack_trim_end=NUM         number of stack frames to trim from the end\n"
"  --stack_trim_start=NUM       number of stack frames to trim from the start\n"
"  --start_demo_name=NAME       starting demo name\n"
"  --window_backend=ARG         OpenGL3 backend\n");
	}

	/* Kaedenn 2019/10/27 */
	bool enableVerbose = false;
	if (args.CheckCmdLineFlag("verbose"))
	{
		enableVerbose = true;
	}
	const char* verboseEnv = getenv("B3_EXAMPLE_BROWSER_VERBOSE");
	if (verboseEnv != NULL && strlen(verboseEnv) > 0)
	{
		enableVerbose = true;
	}
	if (enableVerbose)
	{
		m_internalData->m_verboseMode = true;
		b3Printf("Verbose mode for <%s::%s> is enabled", typeid(*this).name(), __FUNCTION__);
#ifndef BT_NO_PROFILE
		b3Printf("Profiling is enabled");
#else
		b3Printf("Profiling is disabled via BT_NO_PROFILE");
#endif
		for (int i = 0; i < argc; ++i)
		{
			b3Printf("argv[%d] = \"%s\"", i, argv[i]);
		}
	}

	/* Kaedenn 2019/10/27 */
	args.GetCmdLineArgument("stack_trim_start", gStackTrimStart);
	args.GetCmdLineArgument("stack_trim_end", gStackTrimEnd);
	char* trimStrings = NULL;
	if (args.GetCmdLineArgument("stack_trim", trimStrings))
	{
		b3Printf("Parsing stack trim strings \"%s\"", trimStrings);
		int i = 0;
		int last = 0;
		while (trimStrings[i] != '\0')
		{
			if (trimStrings[i] == ';')
			{
				gStackTrimFilters.push_back(std::string(&trimStrings[last], &trimStrings[i]));
				last = i+1;
			}
			++i;
		}
		if (last < i)
		{
			gStackTrimFilters.push_back(std::string(&trimStrings[last]));
		}
		for (i = 0; i < gStackTrimFilters.size(); ++i)
		{
			b3Printf("Stack trim pattern %d: \"%s\"", i, gStackTrimFilters[i].c_str());
		}
	}

	if (args.CheckCmdLineFlag("nogui"))
	{
		renderGrid = false;
		renderGui = false;
	}

	if (args.CheckCmdLineFlag("tracing"))
	{
		b3ChromeUtilsStartTimings();
	}

	args.GetCmdLineArgument("fixed_timestep", gFixedTimeStep);
	args.GetCmdLineArgument("png_skip_frames", gPngSkipFrames);
	///The OpenCL rigid body pipeline is experimental and
	///most OpenCL drivers and OpenCL compilers have issues with our kernels.
	///If you have a high-end desktop GPU such as AMD 7970 or better, or NVIDIA GTX 680 with up-to-date drivers
	///you could give it a try
	///Note that several old OpenCL physics examples still have to be ported over to this new Example Browser
	if (args.CheckCmdLineFlag("enable_experimental_opencl"))
	{
		enable_experimental_opencl = true;
		gAllExamples->initOpenCLExampleEntries();
	}

	if (args.CheckCmdLineFlag("disable_retina"))
	{
		gAllowRetina = false;
	}

	int width = 1024;
	int height = 768;

	if (args.CheckCmdLineFlag("width"))
	{
		args.GetCmdLineArgument("width", width);
	}
	if (args.CheckCmdLineFlag("height"))
	{
		args.GetCmdLineArgument("height", height);
	}

	if (m_internalData->m_verboseMode)
	{
		b3Printf("ExampleBrowser window size: %dx%d pixels", width, height);
	}

#ifndef NO_OPENGL3
	SimpleOpenGL3App* simpleApp = NULL;
	sUseOpenGL2 = args.CheckCmdLineFlag("opengl2");
	args.GetCmdLineArgument("render_device", gRenderDevice);
	args.GetCmdLineArgument("window_backend", gWindowBackend);
#else
	sUseOpenGL2 = true;
#endif
	const char* appTitle = "Bullet Physics ExampleBrowser";
#if defined(_DEBUG) || defined(DEBUG)
	const char* optMode = "Debug build (slow)";
#else
	const char* optMode = "Release build";
#endif

#ifdef B3_USE_GLFW
	const char* glContext = "[glfw]";
#else
	const char* glContext = "[btgl]";
#endif

	if (sUseOpenGL2)
	{
		char title[1024];
		sprintf(title, "%s using limited OpenGL2 fallback %s %s", appTitle, glContext, optMode);
		s_app = new SimpleOpenGL2App(title, width, height);
		s_app->m_renderer = new SimpleOpenGL2Renderer(width, height);
	}

#ifndef NO_OPENGL3
	else
	{
		char title[1024];
		sprintf(title, "%s using OpenGL3+ %s %s", appTitle, glContext, optMode);
		simpleApp = new SimpleOpenGL3App(title, width, height, gAllowRetina, gWindowBackend, gRenderDevice);
		s_app = simpleApp;
	}
#endif
	m_internalData->m_app = s_app;
	char* gVideoFileName = NULL;
	args.GetCmdLineArgument("mp4", gVideoFileName);
#ifndef NO_OPENGL3
	if (gVideoFileName)
	{
		simpleApp->dumpFramesToVideo(gVideoFileName);
	}
#endif

	s_instancingRenderer = s_app->m_renderer;
	s_window = s_app->m_window;

	width = s_window->getWidth();
	height = s_window->getHeight();

	prevMouseMoveCallback = s_window->getMouseMoveCallback();
	s_window->setMouseMoveCallback(MyMouseMoveCallback);

	prevMouseButtonCallback = s_window->getMouseButtonCallback();
	s_window->setMouseButtonCallback(MyMouseButtonCallback);
	prevKeyboardCallback = s_window->getKeyboardCallback();
	s_window->setKeyboardCallback(MyKeyboardCallback);

	s_app->m_renderer->getActiveCamera()->setCameraDistance(13);
	s_app->m_renderer->getActiveCamera()->setCameraPitch(0);
	s_app->m_renderer->getActiveCamera()->setCameraTargetPosition(0, 0, 0);

	float mouseMoveMult = s_app->getMouseMoveMultiplier();
	if (args.GetCmdLineArgument("mouse_move_multiplier", mouseMoveMult))
	{
		s_app->setMouseMoveMultiplier(mouseMoveMult);
	}

	float mouseWheelMult = s_app->getMouseWheelMultiplier();
	if (args.GetCmdLineArgument("mouse_wheel_multiplier", mouseWheelMult))
	{
		s_app->setMouseWheelMultiplier(mouseWheelMult);
	}

	args.GetCmdLineArgument("shared_memory_key", gSharedMemoryKey);

	float red, green, blue;
	s_app->getBackgroundColor(&red, &green, &blue);
	args.GetCmdLineArgument("background_color_red", red);
	args.GetCmdLineArgument("background_color_green", green);
	args.GetCmdLineArgument("background_color_blue", blue);
	s_app->setBackgroundColor(red, green, blue);

	assert(glGetError() == GL_NO_ERROR);

	{
		GL3TexLoader* myTexLoader = new GL3TexLoader;
		m_internalData->m_myTexLoader = myTexLoader;

		if (sUseOpenGL2)
		{
			m_internalData->m_gwenRenderer = new Gwen::Renderer::OpenGL_DebugFont(s_window->getRetinaScale());
		}
#ifndef NO_OPENGL3
		else
		{
			sth_stash* fontstash = simpleApp->getFontStash();
			m_internalData->m_gwenRenderer = new GwenOpenGL3CoreRenderer(simpleApp->m_primRenderer, fontstash, width, height, s_window->getRetinaScale(), myTexLoader);
		}
#endif

		gui2 = new GwenUserInterface();

		m_internalData->m_gui = gui2;

		m_internalData->m_myTexLoader = myTexLoader;

		gui2->init(width, height, m_internalData->m_gwenRenderer, s_window->getRetinaScale());
	}
	//gui = 0;// new GwenUserInterface;

	GL3TexLoader* myTexLoader = m_internalData->m_myTexLoader;
	// = myTexLoader;

	//

	if (gui2)
	{
		//  gui->getInternalData()->m_explorerPage
		Gwen::Controls::TreeControl* tree = gui2->getInternalData()->m_explorerTreeCtrl;

		//gui->getInternalData()->pRenderer->setTextureLoader(myTexLoader);

#ifndef BT_NO_PROFILE
		s_profWindow = setupProfileWindow(gui2->getInternalData());
		m_internalData->m_profWindow = s_profWindow;
		profileWindowSetVisible(s_profWindow, false);
#endif  //BT_NO_PROFILE
		gui2->setFocus();

		s_parameterInterface = s_app->m_parameterInterface = new GwenParameterInterface(gui2->getInternalData());
		s_app->m_2dCanvasInterface = new QuickCanvas(myTexLoader);

		///add some demos to the gAllExamples

		int numDemos = gAllExamples->getNumRegisteredExamples();
		if (m_internalData->m_verboseMode)
		{
			b3Printf("Registered %d examples", numDemos);
		}

		//char nodeText[1024];
		//int curDemo = 0;
		int selectedDemo = 0;
		Gwen::Controls::TreeNode* curNode = tree;
		m_internalData->m_handler2 = new MyMenuItemHander(-1);

		char* demoNameFromCommandOption = NULL;
		args.GetCmdLineArgument("start_demo_name", demoNameFromCommandOption);
		if (demoNameFromCommandOption)
		{
			selectedDemo = -1;
		}

		tree->onReturnKeyDown.Add(m_internalData->m_handler2, &MyMenuItemHander::onButtonD);
		int firstAvailableDemoIndex = -1;
		Gwen::Controls::TreeNode* firstNode = NULL;

		for (int d = 0; d < numDemos; d++)
		{
			//	  sprintf(nodeText, "Node %d", i);
			Gwen::UnicodeString nodeUText = Gwen::Utility::StringToUnicode(gAllExamples->getExampleName(d));
			if (gAllExamples->getExampleCreateFunc(d))  //was test for gAllExamples[d].m_menuLevel==1
			{
				Gwen::Controls::TreeNode* pNode = curNode->AddNode(nodeUText);

				if (firstAvailableDemoIndex < 0)
				{
					firstAvailableDemoIndex = d;
					firstNode = pNode;
				}

				if (d == selectedDemo)
				{
					firstAvailableDemoIndex = d;
					firstNode = pNode;
					//pNode->SetSelected(true);
					//tree->ExpandAll();
					//  tree->ForceUpdateScrollBars();
					//tree->OnKeyLeft(true);
					//  tree->OnKeyRight(true);

					//tree->ExpandAll();

					//  selectDemo(d);
				}

				if (demoNameFromCommandOption)
				{
					const char* demoName = gAllExamples->getExampleName(d);
					if (!strcmp(demoName, demoNameFromCommandOption))
					{
						firstAvailableDemoIndex = d;
						firstNode = pNode;
					}
				}

#if 1
				MyMenuItemHander* handler = new MyMenuItemHander(d);
				m_internalData->m_handlers.push_back(handler);

				pNode->onNamePress.Add(handler, &MyMenuItemHander::onButtonA);
				pNode->GetButton()->onDoubleClick.Add(handler, &MyMenuItemHander::onButtonB);
				pNode->GetButton()->onDown.Add(handler, &MyMenuItemHander::onButtonC);
				pNode->onSelect.Add(handler, &MyMenuItemHander::onButtonE);
				pNode->onReturnKeyDown.Add(handler, &MyMenuItemHander::onButtonG);
				pNode->onSelectChange.Add(handler, &MyMenuItemHander::onButtonF);

#endif
				//		  pNode->onKeyReturn.Add(handler, &MyMenuItemHander::onButtonD);
				//		  pNode->GetButton()->onKeyboardReturn.Add(handler, &MyMenuItemHander::onButtonD);
				//	  pNode->onNamePress.Add(handler, &MyMenuItemHander::onButtonD);
				//		  pNode->onKeyboardPressed.Add(handler, &MyMenuItemHander::onButtonD);
				//		  pNode->OnKeyPress
			}
			else
			{
				curNode = tree->AddNode(nodeUText);
				m_internalData->m_nodes.push_back(curNode);
			}
		}

		if (sCurrentDemo == 0)
		{
			if (firstAvailableDemoIndex >= 0)
			{
				firstNode->SetSelected(true);
				while (firstNode != tree)
				{
					firstNode->ExpandAll();
					firstNode = (Gwen::Controls::TreeNode*)firstNode->GetParent();
				}

				selectDemo(firstAvailableDemoIndex);
			}
		}
		free(demoNameFromCommandOption);
		demoNameFromCommandOption = 0;

		btAssert(sCurrentDemo != 0);
		if (sCurrentDemo == 0)
		{
			printf("Error, no demo/example\n");
			exit(0);
		}

		gui2->registerFileOpenCallback(fileOpenCallback);
		gui2->registerQuitCallback(quitCallback);
		gui2->registerSaveCallback(saveCallback);
	}

	/* Kaedenn 2019/09/28 */
	int paused = 0;
	if (args.CheckCmdLineFlag("paused"))
	{
		pauseSimulation = true;
	}

	/* Kaedenn 2019/10/13 */
	args.GetCmdLineArgument("png_prefix", gPngFilePrefix);

	char* savePath = NULL;
	args.GetCmdLineArgument("save_bullet", savePath);
	if (savePath && strlen(savePath) > 0)
	{
		strncpy(saveFileName, savePath, sizeof(saveFileName)/sizeof(*saveFileName));
	}
	else
	{
		strcpy(saveFileName, startSaveFileName);
	}

	/* Kaedenn 2019/10/25 */
	if (args.CheckCmdLineFlag("hide_explorer"))
	{
		if (gui2->getInternalData() && gui2->getInternalData()->m_windowLeft)
		{
			gui2->getInternalData()->m_windowLeft->Hide();
		}
	}

	return true;
}

CommonExampleInterface* OpenGLExampleBrowser::getCurrentExample()
{
	btAssert(sCurrentDemo);
	return sCurrentDemo;
}

bool OpenGLExampleBrowser::requestedExit()
{
	return s_window->requestedExit();
}

void OpenGLExampleBrowser::updateGraphics()
{
	if (sCurrentDemo)
	{
		if (!pauseSimulation || singleStepSimulation)
		{
			//B3_PROFILE("sCurrentDemo->updateGraphics");
			sCurrentDemo->updateGraphics();
		}
	}
}

void OpenGLExampleBrowser::update(float deltaTime)
{

	b3ChromeUtilsEnableProfiling();

	if (!gEnableRenderLoop && !singleStepSimulation)
	{
		B3_PROFILE("updateGraphics");
		sCurrentDemo->updateGraphics();
		return;
	}

	B3_PROFILE("OpenGLExampleBrowser::update");
	//assert(glGetError() == GL_NO_ERROR);
	{
		B3_PROFILE("s_instancingRenderer");
		s_instancingRenderer->init();
	}
	DrawGridData dg;
	dg.upAxis = s_app->getUpAxis();

	{
		BT_PROFILE("Update Camera and Light");

		s_instancingRenderer->updateCamera(dg.upAxis);
	}

	static int frameCount = 0;
	frameCount++;

	if ((gDebugDrawFlags & btIDebugDraw::DBG_DrawFrames) != 0)
	{
		BT_PROFILE("Draw frame counter");
		char bla[1024];
		sprintf(bla, "Frame %d", frameCount);
		s_app->drawText(bla, 10, 10);
	}

	if (gPngFileName)
	{
		static int skip = 0;
		skip--;
		if (skip < 0)
		{
			skip = gPngSkipFrames;
			//printf("gPngFileName=%s\n",gPngFileName);
			static int s_frameCount = 0;

			if (gPngFilePrefix)
			{
				sprintf(staticPngFileName, "%s%s-%d.png", gPngFilePrefix, gPngFileName, s_frameCount++);
			}
			else
			{
				sprintf(staticPngFileName, "%s-%d.png", gPngFileName, s_frameCount++);
			}
			b3Printf("Made screenshot %s", staticPngFileName);
			s_app->dumpNextFrameToPng(staticPngFileName);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
	}

	if (sCurrentDemo)
	{
		if (!pauseSimulation || singleStepSimulation)
		{
			//printf("---------------------------------------------------\n");
			//printf("Framecount = %d\n",frameCount);
			B3_PROFILE("sCurrentDemo->stepSimulation");

			if (gFixedTimeStep > 0)
			{

				sCurrentDemo->stepSimulation(gFixedTimeStep);
			}
			else
			{
				sCurrentDemo->stepSimulation(deltaTime);  //1./60.f);
			}
		}

		if (renderGrid)
		{
			BT_PROFILE("Draw Grid");
			//glPolygonOffset(3.0, 3);
			//glEnable(GL_POLYGON_OFFSET_FILL);
			s_app->drawGrid(dg);
		}
		if (renderVisualGeometry && ((gDebugDrawFlags & btIDebugDraw::DBG_DrawWireframe) == 0))
		{
			if (visualWireframe)
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			}
			BT_PROFILE("Render Scene");
			sCurrentDemo->renderScene();
		}
		else
		{
			B3_PROFILE("physicsDebugDraw");
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			sCurrentDemo->physicsDebugDraw(gDebugDrawFlags);
		}
	}

	if (gui2 && s_guiHelper && s_guiHelper->getRenderInterface() && s_guiHelper->getRenderInterface()->getActiveCamera())
	{
		B3_PROFILE("setStatusBarMessage");
		CommonCameraInterface* ci = s_guiHelper->getRenderInterface()->getActiveCamera();
		char msg[1024] = {0};
		float camDist = ci->getCameraDistance();
		float pitch = ci->getCameraPitch();
		float yaw = ci->getCameraYaw();
		float camTarget[3] = {0};
		float camPos[3] = {0};
		ci->getCameraPosition(camPos);
		ci->getCameraTargetPosition(camTarget);
		snprintf(msg, sizeof(msg), "camTargetPos=%2.2f,%2.2f,%2.2f, dist=%2.2f, pitch=%2.2f, yaw=%2.2f", camTarget[0], camTarget[1], camTarget[2], camDist, pitch, yaw);
		gui2->setStatusBarMessage(msg, true);
	}

	if (renderGui)
	{
		B3_PROFILE("renderGui");

#ifndef BT_NO_PROFILE
		if (!pauseSimulation || singleStepSimulation)
		{
			if (isProfileWindowVisible(s_profWindow))
			{
				processProfileData(s_profWindow, false);
			}
		}
#endif  //#ifndef BT_NO_PROFILE

		{
			B3_PROFILE("updateOpenGL");
			if (sUseOpenGL2)
			{
				saveOpenGLState(s_instancingRenderer->getScreenWidth() * s_window->getRetinaScale(), s_instancingRenderer->getScreenHeight() * s_window->getRetinaScale());
			}

			if (m_internalData->m_gui)
			{
				gBlockGuiMessages = true;
				m_internalData->m_gui->draw(s_instancingRenderer->getScreenWidth(), s_instancingRenderer->getScreenHeight());
				gBlockGuiMessages = false;
			}

			if (sUseOpenGL2)
			{
				restoreOpenGLState();
			}
		}
	}

	singleStepSimulation = false;

	{
		BT_PROFILE("Sync Parameters");
		if (s_parameterInterface)
		{
			s_parameterInterface->syncParameters();
		}
	}
	{
		BT_PROFILE("Swap Buffers");
		s_app->swapBuffer();
	}

	if (gui2)
	{
		B3_PROFILE("forceUpdateScrollBars");
		gui2->forceUpdateScrollBars();
	}
}

void OpenGLExampleBrowser::setSharedMemoryInterface(class SharedMemoryInterface* sharedMem)
{
	gDisableDemoSelection = true;
	sSharedMem = sharedMem;
}
