#include "Application.h"

#define SAMPLE_RATE 48000 //44100
#define FRAMES_PER_BUFFER 512

const int NUM_SEGMENTS = 8;//24; // 3 * 8

static const float THRESHOLD = 0.001;

std::vector<float> audioData(8, 0.0f); // 8 channels
std::mutex audioDataMutex; // protects audioData from being concurrently accessed by multiple threads.

//These are needed for OpenGL objects
GLuint vao;
GLuint vbo;
GLuint VBOs[8], VAOs[8];

template<typename T>
constexpr const T& clamp(const T& value, const T& min, const T& max) {
    return value < min ? min : (value > max ? max : value);
}

Application::Application() : window(nullptr), showMenu(true) {}

static void checkErr(PaError err) {
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
}

static int patestCallback(
    const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
    void* userData
) {
    float* in = (float*)inputBuffer;
    (void)outputBuffer;

    if (inputBuffer == NULL) {
        std::cerr << "Input buffer is NULL" << std::endl;
        return paContinue;
    }

    // Store the maximum absolute value for each channel
    std::lock_guard<std::mutex> lock(audioDataMutex);
    for (int channel = 0; channel < 8; ++channel) {
        float maxValue = 0.0f;
        for (unsigned long frame = 0; frame < framesPerBuffer; ++frame) {
            float value = in[frame * 8 + channel];
            if (fabs(value) > maxValue) {
                maxValue = fabs(value);
            }
        }
        audioData[channel] = maxValue;
    }

    std::cout << "audioData values: ";
    for (int channel = 0; channel < 8; ++channel) {
        std::cout << audioData[channel] << " ";
    }
    std::cout << std::endl;

    return paContinue;
}

int Application::SetupPortAudio()
{
    PaError err;
    err = Pa_Initialize();
    checkErr(err);

    int numDevices = Pa_GetDeviceCount();
    printf("Number of devices: %d\n", numDevices);

    if (numDevices < 0) {
        printf("Error getting device count.\n");
        exit(EXIT_FAILURE);
    }
    else if (numDevices == 0) {
        printf("There are no available audio devices on this machine.\n");
        exit(EXIT_SUCCESS);
    }

    const PaDeviceInfo* deviceInfo;
    for (int i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxInputChannels > 0)
        {
            printf("Device %d:\n", i);
            printf("  name: %s\n", deviceInfo->name);
            printf("  maxInputChannels: %d\n", deviceInfo->maxInputChannels);
            printf("  maxOutputChannels: %d\n", deviceInfo->maxOutputChannels);
            printf("  defaultSampleRate: %f\n", deviceInfo->defaultSampleRate);
        }
    }

    /*
  have to use this device:
      Name: CABLE Output (VB-Audio Virtual
      maxInputChannels: 8
      maxOutputChannels: 0
      defaultSampleRate: 44100.000000
  */

    int device = 2;

    PaStreamParameters inputParameters;
    PaStreamParameters outputParameters;

    memset(&inputParameters, 0, sizeof(inputParameters));
    inputParameters.channelCount = 8;
    inputParameters.device = device;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(device)->defaultLowInputLatency;

    PaStream* stream;
    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        NULL,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paNoFlag,
        patestCallback,
        NULL
    );
    checkErr(err);

    err = Pa_StartStream(stream);
    checkErr(err);

    //Pa_Sleep(10 * 1000);

    //err = Pa_StopStream(stream);
    //checkErr(err);

    /*err = Pa_CloseStream(stream);
    checkErr(err)*/;

    //err = Pa_Terminate();
    //checkErr(err);

    return EXIT_SUCCESS;
}

void CreateCircleQuads(GLfloat* vertices, GLfloat* colours, float centerX, float centerY, float innerRadius, float outerRadius, int numSegments) {
    float angleStep = 2.0f * 3.1415926f / numSegments;
    float rotationAngle = 3.1415926f / 1.6f; // rotation angle because the octagon isnt straight

    for (int i = 0; i < numSegments; ++i) {
        float theta1 = i * angleStep + rotationAngle;
        float theta2 = (i + 1) * angleStep + rotationAngle;

        float innerX1 = centerX + innerRadius * cosf(theta1);
        float innerY1 = centerY + innerRadius * sinf(theta1);
        float outerX1 = centerX + outerRadius * cosf(theta1);
        float outerY1 = centerY + outerRadius * sinf(theta1);

        float innerX2 = centerX + innerRadius * cosf(theta2);
        float innerY2 = centerY + innerRadius * sinf(theta2);
        float outerX2 = centerX + outerRadius * cosf(theta2);
        float outerY2 = centerY + outerRadius * sinf(theta2);

        // 1st triangle 
        vertices[i * 12] = innerX1;
        vertices[i * 12 + 1] = innerY1;
        vertices[i * 12 + 2] = outerX1;
        vertices[i * 12 + 3] = outerY1;
        vertices[i * 12 + 4] = innerX2;
        vertices[i * 12 + 5] = innerY2;

        // 2nd triangle 
        vertices[i * 12 + 6] = outerX1;
        vertices[i * 12 + 7] = outerY1;
        vertices[i * 12 + 8] = outerX2;
        vertices[i * 12 + 9] = outerY2;
        vertices[i * 12 + 10] = innerX2;
        vertices[i * 12 + 11] = innerY2;

        // Set all colours to red with initial alpha value of 1.0f
        for (int j = 0; j < 6; ++j) {
            colours[i * 24 + j * 4] = 1.0f; // R
            colours[i * 24 + j * 4 + 1] = 0.0f; // G
            colours[i * 24 + j * 4 + 2] = 0.0f; // B
            colours[i * 24 + j * 4 + 3] = 1.0f; // Initial alpha value
        }
    }
}

void Application::Setup() {

    const int numSegments = 8; // 24; // for when i get 8 quads working then i can interpolate between three for each edge, just so it looks more like a cirle!
    const float innerRadius = 0.48f;
    const float outerRadius = 0.5f;

    // Allocate memory with correct sizes
    GLfloat* vertices = new GLfloat[numSegments * 12];
    GLfloat* colours = new GLfloat[numSegments * 24]; // RGBA

    CreateCircleQuads(vertices, colours, 0.0f, 0.0f, innerRadius, outerRadius, numSegments);

    glGenVertexArrays(1, &vao);
    glGenBuffers(2, VBOs);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, VBOs[0]);
    glBufferData(GL_ARRAY_BUFFER, numSegments * 12 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, VBOs[1]);
    glBufferData(GL_ARRAY_BUFFER, numSegments * 24 * sizeof(GLfloat), colours, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)0); // RGBA
    glEnableVertexAttribArray(1);

    delete[] vertices;
    delete[] colours;
}

void Application::DrawApplication() {
    std::vector<GLfloat> colours(NUM_SEGMENTS * 24, 0.0f); // 24 per segment for RGBA

    std::lock_guard<std::mutex> lock(audioDataMutex);
    for (int i = 0; i < NUM_SEGMENTS; ++i) {
        float maxValue = audioData[i % 8]; // Use the audio data for transparency

        // Calculate alpha based on maxValue
        float alpha = maxValue;
        alpha = clamp(alpha, 0.0f, 1.0f); // so alpha is within the valid range 0 and 1

        for (int j = 0; j < 6; ++j) {
            int baseIndex = (i * 6 + j) * 4; // Update the base index calculation
            colours[baseIndex] = quadColour.x; // Red
            colours[baseIndex + 1] = quadColour.y; // Green
            colours[baseIndex + 2] = quadColour.z; // Blue
            colours[baseIndex + 3] = alpha; // Set the alpha value
        }

    }

    // Now update the colour buffer on the GPU
    glBindBuffer(GL_ARRAY_BUFFER, VBOs[1]);
    glBufferData(GL_ARRAY_BUFFER, colours.size() * sizeof(GLfloat), colours.data(), GL_DYNAMIC_DRAW);

    // Bind the VAO and draw
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6 * NUM_SEGMENTS);
}

void Application::InitApplication()
{
    //Application::SetupPortAudio();

    ///// ----- THE SETUP PHASE, IN THE MAIN FUNCTION -----
    /// --- LIBRARY SETUP ---

    //Initializes the GLFW library, 
    //Needs to be the first GLFW function called 
    //as it sets up data structures required by later GLFW functions
    glfwInit();

    //Use a modern version of OpenGL - 4.3, 
    //The machine this runs on must be compatible with this version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    /*
        Configures the window we want to use
            Paramaters are (width of window,
            height of window,
            title of window,
            NULL for windowed mode, NULL for sharing resources)

        Also creates an OpenGL context which is
        associated with the window (we only have 1 window at the moment)
    */

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    int widthW = glfwGetVideoMode(monitor)->width;
    int heightW = glfwGetVideoMode(monitor)->height;

    glfwWindowHint(GLFW_FLOATING, true);
    glfwWindowHint(GLFW_RESIZABLE, false);                   // stops the window from being resized
    glfwWindowHint(GLFW_DECORATED, false);                   // gets rid of the top of the window
    glfwWindowHint(GLFW_MAXIMIZED, true);                    // makes the window full screen
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true);      // makes the 'overlay'

    window = glfwCreateWindow(widthW, heightW, "Visualisation Application", NULL, NULL);

    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
        std::cout << "click happened " << button << std::endl;
        });

    /*
        While not needed for the running of the program,
        this will help us in case there is an error with using GLFW
        The return -1 will stop the program and give us a readable error message
    */
    if (!window)
    {
        std::cout << "ERROR: No window was created!" << std::endl;
        //return -1;
    }

    /*
        Make the OpenGL context of the window made above current
            The current context is the one that processes any commands
            given to it as well as holding OpenGL state / information
    */
    glfwMakeContextCurrent(window);

    /*
        Initialize the GL3W library (after the OpenGL context has been created)
            This library is a OpenGL Extension Loading Library,
                which loads pointers to the OpenGL functions
            This is needed to access OpenGL 1.1+ functions,
                of which we will be using

        We could just use gl3wInit() on it's own, but this way,
        we can check for errors with using GL3W

        gl3wInit() returns a 0 if everything is OK (aka GL3W initializes successfully)
        and a non-zero value if something is wrong.
        if(0) will not run the inside of the if statement,
        but if(non-zero) will stop the program and give us a readable error message
    */
    if (gl3wInit())
    {
        std::cout << "ERROR: Failed to initialize OpenGL" << std::endl;
        //return -1;
    }


    //How often the buffer should swap - 1 = do everytime there is a screen update
    glfwSwapInterval(1);


    /// --- OPENGL SHADER SETUP ---

    //Create shader programs for modern OpenGL
    const char* vertexShaderCode =
    {
        "#version 330 core\n \
    \n \
    layout(location = 0) in vec2 vertexPosition; \n \
    layout(location = 1) in vec4 vertexcolour; \n \
    out vec3 fragcolour; \n \
    out float fragAlpha; \n \
    \n \
    void main()\n \
    {\n \
        gl_Position = vec4(vertexPosition, 0.0, 1.0);\n \
        fragcolour = vertexcolour.rgb; \n \
        fragAlpha = vertexcolour.a; \n \
    \n \
    } \
    "
    };

    const char* fragmentShaderCode =
    {
        "#version 330 core\n \
    \n \
    in vec3 fragcolour; \n \
    in float fragAlpha; \n \
    layout(location = 0) out vec4 colour; \n \
    \n \
    void main()\n \
    {\n \
        colour = vec4(fragcolour, fragAlpha);\n \
    \n \
    } \
    "
    };

    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderId, 1, &vertexShaderCode, NULL);
    glCompileShader(vertexShaderId);

    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderId, 1, &fragmentShaderCode, NULL);
    glCompileShader(fragmentShaderId);

    //As modern OpenGL uses the GPU
    //Send the shader programs to the GPU
    GLuint programId = glCreateProgram();
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);
    glLinkProgram(programId);
    glUseProgram(programId);



    //Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking for tabs hehe:)

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ///// ----- THE Application LOOP PHASE -----

    /// ---- STILL IN THE MAIN FUNCTION, AFTER THE SETUP STAGE
    ///// ----- THE Application LOOP PHASE -----

    //A loop that allows the window to refresh and display content. 
    //In time, it will check for user input and draw things.

    bool showMenu = true;

    //if the window should close, this will be while(false) 
    //and leave the while loop. Otherwise, continue looping.
    Application::Setup();
    //while (!glfwWindowShouldClose(window))
    //{
    //    glfwPollEvents();

    //    //clear the framebuffer/window to a background colour
    //    const float backgroundColour[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    //    glClearBufferfv(GL_COLOR, 0, backgroundColour);

    //    //pull out the audio steam func and turn into a function to print the float values.

    //    Application::DrawApplication();


    //    if (GetAsyncKeyState(VK_INSERT) & 1) {
    //        showMenu = !showMenu;
    //        if (showMenu) {
    //            ShowWindow(window);
    //        }
    //        else {
    //            HideWindow(window);
    //        }
    //    }

    //    if (showMenu) {

    //        ImGui_ImplOpenGL3_NewFrame();
    //        ImGui_ImplGlfw_NewFrame();
    //        ImGui::NewFrame();

    //        ImGui::Begin("Instructions");
    //        ImGui::Text("To toggle mouse input press the insert key.");
    //        ImGui::End();

    //        ImGui::Begin("Help");
    //        ImGui::Text("Make sure you have set the Display mode in the games settings to 'Windowed Fullscreen'.");
    //        ImGui::End();

    //        ImGui::Begin("Colour settings for the circle.");
    //        ImGui::ColorEdit3("Colour", (float*)&quadColour);
    //        ImGui::End();

    //        ImGui::Render();
    //        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    //    }

    //    glfwSwapBuffers(window);
    //}

    //glfwDestroyWindow(window);
    //ImGui_ImplGlfw_Shutdown();
    //ImGui::DestroyContext();
}

void ShowWindow(GLFWwindow* window) {
    std::cout << "showing" << std::endl;
    glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);
}
void HideWindow(GLFWwindow* window) {
    std::cout << "hidden" << std::endl;
    glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
}

void Application::RunApplication()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        const float backgroundColour[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearBufferfv(GL_COLOR, 0, backgroundColour);

        DrawApplication();

        if (GetAsyncKeyState(VK_INSERT) & 1) {
            showMenu = !showMenu;
            if (showMenu) {
                ShowWindow(window);
            }
            else {
                HideWindow(window);
            }
        }

        if (showMenu) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Instructions");
            ImGui::Text("To toggle mouse input press the insert key.");
            ImGui::End();

            ImGui::Begin("Help");
            ImGui::Text("Make sure you have set the Display mode in the games settings to 'Windowed Fullscreen'.");
            ImGui::End();

            ImGui::Begin("Colour settings for the circle.");
            ImGui::ColorEdit3("Colour", (float*)&quadColour);
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        glfwSwapBuffers(window);
    }
}

void Application::Cleanup()
{
    if (window)
    {
        glfwDestroyWindow(window);
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
    }
}
