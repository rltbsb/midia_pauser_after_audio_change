# midia_pauser_after_audio_change

Windows 10 64-bit C utility that sends an SMTC pause command whenever the default audio output device changes.

## Building from the Command Line (recommended)

1. Open the **Developer Command Prompt for VS 2022** or run `vcvars64.bat`.
2. Compile the resource script to a `.res` file:

   ```
   rc /nologo pauser.rc
   ```

3. Compile and link the program with the resource file:

   ```
   cl /DUNICODE /D_UNICODE /W3 pauser.c pauser.res
   ```

   The `#pragma comment` directives link the required libraries automatically.

## Building with Visual Studio Community

1. Open Visual Studio 2022.
2. Create an empty **Windows Application (.exe)** project.
3. Add `pauser.c` **and** `pauser.rc` to the project so the icon resources (`icon_on.ico` and `icon_off.ico`) are compiled into the executable.
4. Build and run. The application sits in the system tray and sends a **pause** command whenever the default output device changes.

The source defines the required COM GUIDs explicitly to avoid unresolved external symbols.

## Usage

- Left-click the tray icon to toggle between active (blue background) and inactive (blue with a red **X** overlay).
- Right-click the tray icon to open the context menu with **On/Off** and **Exit** options.
