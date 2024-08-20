#include "Includes.h" // So it's not so crowded

class Application
{
public: 
	Application();
	~Application() { Cleanup(); };

	void InitApplication();                             // Setup 
	void RunApplication();							    // Application loop
	void Cleanup();						                // Shuts down application
private:
	GLFWwindow* window;
	bool showMenu;
	ImVec4 quadColour = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); //RGB values

	int	 SetupPortAudio();                              // Setup for PortAudio
	void Setup();                                       // Setup for the quads
	void DrawApplication(void);                         // Draws the quads.
};