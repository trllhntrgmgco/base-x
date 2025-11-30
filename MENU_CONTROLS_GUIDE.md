# Base-X Game Engine - Menu Controls Guide

## Enhanced Menu System

The Base-X Game Engine now includes comprehensive menu controls for managing demos and settings.

### Main Menu Structure

#### 1. **File Menu**
- Exit - Close the application

#### 2. **Demos Menu**
- **Systems Demo** (Ctrl+1) - Load the systems demonstration
- **Graphics Demo** (Ctrl+2) - Load the graphics demonstration  
- **Physics Demos** - Submenu with physics options:
  - **Physics Ball** - Bouncing ball demo
  - **Physics Gravity** - Gravity simulation demo
  - **Physics Collision** - Advanced collision detection demo
  - **Reset Physics** (Ctrl+R) - Reset current physics demo
  - **Pause/Resume** (Ctrl+P) - Pause or resume the current demo
- **Audio Demo** (Ctrl+4) - Load the audio demonstration

#### 3. **View Menu**
- **800x600** - Set window to 800x600 resolution
- **1024x768** - Set window to 1024x768 resolution
- **1280x720** - Set window to 1280x720 resolution (720p)
- **1920x1080** - Set window to 1920x1080 resolution (1080p)
- **Fullscreen** (F11) - Toggle fullscreen mode (ESC to exit)

#### 4. **Settings Menu**
- **General Settings** - Open general configuration dialog
- **Physics Settings** - Open physics parameters dialog

#### 5. **Help Menu**
- **About** - Display application information

### Physics Settings Dialog

The Physics Settings dialog allows you to adjust:
- **Gravity** - Gravitational force applied to objects (default: 500)
- **Bounce Factor** - How much objects bounce when colliding (default: 0.8)
- **Friction** - Friction applied when objects hit surfaces (default: 0.95)

Changes apply immediately when you click "Apply" or "OK", and take effect when you reset or switch demos.

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+1 | Switch to Systems Demo |
| Ctrl+2 | Switch to Graphics Demo |
| Ctrl+3 | Switch to Physics Demo |
| Ctrl+4 | Switch to Audio Demo |
| Ctrl+R | Reset Current Physics Demo |
| Ctrl+P | Pause/Resume Demo |
| F11 | Toggle Fullscreen |
| ESC | Exit Fullscreen (when in fullscreen mode) |

### Tab Control

The application also includes a tab control at the top for quick demo switching:
- **Systems** - System performance and FPS demonstration
- **Graphics** - Graphics rendering demonstration
- **Physics** - Physics simulation (cycles through Ball, Gravity, Collision)
- **Audio** - Audio processing demonstration

### Features

#### Demo Management
- **Easy switching** between different demo categories
- **Specific physics demo selection** through the Physics submenu
- **Reset functionality** to restart demos with new settings
- **Pause/Resume** capability for better control

#### Display Options
- **Multiple resolution presets** for different screen sizes
- **Fullscreen mode** for immersive viewing
- **Automatic control hiding** in fullscreen mode

#### Settings Management
- **Real-time physics parameter adjustment**
- **Settings persistence** across demo switches
- **Apply/OK/Cancel** dialog controls

#### User Experience
- **Keyboard shortcuts** for power users
- **Visual feedback** showing current settings in physics demos
- **Help text** displayed in demos showing available controls
- **Collision highlighting** in physics demos
- **Velocity vectors** shown for moving objects

### Technical Implementation

The menu system is implemented using:
- **Windows Resource files** (.rc) for menu definitions
- **Message handling** in the main window procedure
- **Dialog boxes** for settings configuration
- **Global variable sharing** between main application and demos
- **State management** for fullscreen, pause, and current demo tracking

This enhanced menu system provides users with complete control over the game engine demonstrations and allows for easy experimentation with different physics parameters and viewing options.