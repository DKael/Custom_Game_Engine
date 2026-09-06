#include <windows.h>

// D3D 사용에 필요한 라이브러리들을 링크
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// D3D 사용에 필요한 헤더파일들을 포함
#include "URenderer.h"
#include "UDiagram.h"
#include "UBar.h"
#include "Bar.h"
#include "UBall.h"
#include "Sphere.h"
#include "Bullet.h"
#include "Util.h"
#include "USceneManager.h"
#include "UScene.h"

// ImGui 관련 헤더
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"
#include "UInputManager.h"
#include "USoundManager.h"
#include "UGameManager.h"


#pragma comment(lib, "runtimeobject.lib")
// 윈도우의 입력 이벤트를 ImGui에 전달하고, ImGui가 사용했는지 여부를 알려주는 함수
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/// <summary>
/// 윈도우에 이벤트가 발생할 경우 처리되는 콜백 함수
/// </summary>
/// <param name="hWnd"> 어떤 윈도우에 대한 메시지인지 </param>
/// <param name="message"> 어떤 이벤트인지 (종료, 키 입력, 마우스 입력 등) </param>
/// <param name="wParam"> 메시지에 딸려오는 추가 정보 </param>
/// <param name="lParam"> 메시지에 딸려오는 추가 정보 </param>
/// <returns></returns>
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // ImGui의 이벤트 입력인지 판별
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
    case WM_DESTROY:
        // 윈도우가 닫힐 때
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam); // 윈도우 기본 처리 방식
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{

    //// 콘솔 창 생성
    //AllocConsole();

    //// 표준 출력을 콘솔로 연결 (printf나 std::cout을 쓰기 위해)
    //FILE* pFile;
    //freopen_s(&pFile, "CONOUT$", "w", stdout);
    // 윈도우 클래스 이름
    WCHAR WindowClass[] = L"JungleWindowClass";

    // 윈도우 타이틀바에 표시될 이름
    WCHAR Title[] = L"Game Tech Lab";

    // 이 클래스 이름(WindowClass)으로 생성된 모든 윈도우의 메시지는 WndProc()에서 처리된다.
    WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

    // 위에서 만든 wndclass 라는 윈도우 클래스를 등록
    RegisterClassW(&wndclass);

    // 1024 x 1024 크기에 윈도우 생성
    HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
        nullptr, nullptr, hInstance, nullptr);

    bool bIsExit = false;

    /* 생성하는 코드를 여기에 추가합니다. */

    // Renderer Class를 생성
    URenderer renderer;

    // D3D11을 사용한 Renderer 생성
    renderer.Create(hWnd);

    // Renderer 생성 직후, Shader 생성
    renderer.CreateShader();

    // Constant Buffer 생성
    renderer.CreateConstantBuffer();
    renderer.CreateRectBuffer();

    //render에서 초기화
    // Renderer와 Shader 생성 이후, vertexBuffer 생성
    //renderer.NumVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
    //renderer.vertexBufferSphere = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
    renderer.NumVerticesBar = sizeof(bar_vertices) / sizeof(FVertexSimple);
    renderer.vertexBufferBar = renderer.CreateVertexBuffer(bar_vertices, sizeof(bar_vertices));
    renderer.NumVerticesBullet = sizeof(bullet_vertices) / sizeof(FVertexSimple);
    renderer.vertexBufferBullet = renderer.CreateVertexBuffer(bullet_vertices, sizeof(bullet_vertices));


    int SegmentCount = 24;
    int BallVertexCount = SegmentCount * 3;
    FVertexSimple* BallVertices = new FVertexSimple[BallVertexCount];
     
    UBall::CreateCircleVertices(
        BallVertices,
        SegmentCount,
        1.0f,

        0.95f, 0.95f, 1.0f, 1.0f,

        0.65f, 0.68f, 0.75f, 1.0f
    );
    renderer.NumVerticesNewSphere = BallVertexCount; // 직접 72를 대입하거나 변수를 쓰세요.
    renderer.vertexBufferNewSphere = renderer.CreateVertexBuffer(BallVertices, sizeof(FVertexSimple) * BallVertexCount);

    // ImGui를 사용하기 위한 초기화
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_Init((void*)hWnd);
    ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);

    // FPS 제한
    const int targetFPS = 60;
    const double targetFrameTime = 1000.0 / targetFPS; // 한 프레임의 목표 시간 (밀리초 단위)
    const float dt = 1.0f / static_cast<float>(targetFPS);

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER startTime, endTime;
    double elapsedTime = 0.0;

    USoundManager::GetInstance().Init();
    //게임씬 초기화
    USceneManager& sceneManager = USceneManager::GetInstance();
    sceneManager.LoadScene(ESceneType::Title);

    // Main Loop (Quit Message가 들어오기 전까지 아래 Loop를 무한히 실행하게 됨)
    while (bIsExit == false)
    {
        // 메인 루프 시작 시간 기록
        QueryPerformanceCounter(&startTime);

        MSG msg;

        // 처리할 메시지가 더 이상 없을때 까지 수행
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // 키 입력 메시지를 번역
            TranslateMessage(&msg);

            // 메시지를 적절한 윈도우 프로시저에 전달, 메시지가 위에서 등록한 WndProc 으로 전달됨
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)             // 종료 시
            {
                bIsExit = true;
                break;
            }

            //if (msg.wParam == VK_LEFT)
            //    sceneManager.LoadScene(ESceneType::Title);
            //if (msg.wParam == VK_RIGHT)
            //    sceneManager.LoadScene(ESceneType::InGame);

            //if (msg.wParam == VK_RIGHT) Bar.Direction = 1;
        //}
        //else if (msg.message == WM_KEYUP) {
        //    Bar.Direction = 0; // 키를 떼면 멈춤
        //}
        }

        USoundManager::GetInstance().Update();
        UInputManager::GetInstance()->Update();
        ////////////////////////////////////////////
        // 매번 실행되는 코드를 여기에 추가합니다.
        UScene* currentScene = sceneManager.GetCurrentScene();
        currentScene->Update(dt);

        // 준비 작업
        renderer.Prepare();
        renderer.PrepareShader();
        currentScene->Render(renderer);
        currentScene->UIRender();

        // 다 그렸으면 버퍼 스왑

        renderer.SwapBuffer();

        ////////////////////////////////////////////

        // 루프 실행 시간에 따라 FPS 제한
        do
        {
            Sleep(0);

            // 루프 종료 시간 기록
            QueryPerformanceCounter(&endTime);

            // 한 프레임이 소요된 시간 계산 (밀리초 단위로 변환)
            elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;

        } while (elapsedTime < targetFrameTime);
    }

    USoundManager::GetInstance().Release();
    USceneManager::GetInstance().Release();
    UInputManager::GetInstance()->Release();
    UGameManager::GetInstance()->Release();

    delete BallVertices;

    // vertexBuffer 릴리즈
    renderer.ReleaseVertexBuffer();

    // Constant Buffer 릴리즈
    renderer.ReleaseRectBuffer();
    renderer.ReleaseConstantBuffer();

    // 렌더러 소멸 직전, 쉐이더 소멸
    renderer.ReleaseShader();

    ID3D11Debug* debugDevice = nullptr;
    HRESULT hr = renderer.Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&debugDevice);

    if (SUCCEEDED(hr))
    {
        debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

        debugDevice->Release();
    }

    // 렌더러 소멸
    renderer.Release();

    return 0;
}