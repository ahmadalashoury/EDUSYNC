![build](https://github.com/ahmadalashoury/EduSync/actions/workflows/build.yml/badge.svg)


# EduSync - AI-Powered Calendar Application

EduSync is a modern, AI-powered calendar application built with Qt and C++. It features a beautiful dark theme, intelligent event management, and AI-driven scheduling suggestions.

## Features

### 🎨 Modern UI Design
- Beautiful dark theme with modern styling
- Responsive layout with split panels
- Smooth animations and transitions
- Intuitive navigation and controls

### 📅 Calendar Management
- Full calendar view with month navigation
- Date selection and highlighting
- Today button for quick navigation
- Visual event indicators

### 📝 Event Management
- Create, edit, and delete events
- Rich event details (title, description, time, color)
- Event categories and color coding
- Double-click to edit events

### 🤖 AI-Powered Features
- **Smart Suggestions**: AI analyzes your schedule and suggests relevant events
- **Schedule Analysis**: Get insights about your calendar patterns
- **Intelligent Planning**: AI recommends study sessions, breaks, and activities
- **Context-Aware**: Suggestions based on existing events (exams, assignments, etc.)

### 💾 Data Persistence
- Automatic saving to JSON files
- Cross-session data retention
- Reliable data storage

## Screenshots

The application features:
- **Main Window**: Calendar view with event list and AI analysis panel
- **Event Dialog**: Rich event creation and editing interface
- **AI Analysis**: Real-time schedule analysis and recommendations
- **Modern Styling**: Dark theme with blue accent colors

## Requirements

- Qt 6.0 or higher
- CMake 3.16 or higher
- C++17 compatible compiler
- macOS, Windows, or Linux

## Building

### Prerequisites
1. Install Qt 6 (with Qt Creator or standalone)
2. Install CMake
3. Ensure your compiler supports C++17

### Build Instructions

```bash
# Clone or navigate to the project directory
cd EduSync

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build .

# Run the application
./EduSync
```

### Using Qt Creator
1. Open the project in Qt Creator
2. Configure the project (select Qt 6 kit)
3. Build and run

## Usage

### Adding Events
1. Click the "Add Event" button
2. Fill in event details (title, description, time, color)
3. Choose a category
4. Click "Save"

### AI Features
1. **Get Suggestions**: Click "Get AI Suggestions" to see AI-recommended events
2. **Schedule Analysis**: The AI panel shows real-time analysis of your schedule
3. **Smart Planning**: AI suggests study sessions, breaks, and activities based on your existing events

### Managing Events
- **Edit**: Double-click any event in the list
- **Delete**: Select an event and click "Delete Selected"
- **View**: Click on calendar dates to see events for that day

## AI Features Explained

### Smart Suggestions
The AI analyzes your existing events and suggests:
- **Study Sessions**: Based on upcoming exams or tests
- **Break Time**: To prevent burnout
- **Exercise**: For physical well-being
- **Review Sessions**: For academic preparation

### Schedule Analysis
The AI provides insights about:
- Event distribution across categories
- Study vs. break balance
- Upcoming important dates
- Recommendations for improvement

## Technical Details

### Architecture
- **MainWindow**: Main application window with UI layout
- **CalendarWidget**: Custom calendar display with event visualization
- **EventManager**: Handles event CRUD operations and persistence
- **AIPlanner**: AI logic for suggestions and analysis
- **EventDialog**: Event creation and editing interface

### Data Storage
- Events are stored in JSON format
- Automatic saving on changes
- Cross-platform data location

### AI Implementation
- Local AI logic (no external API required)
- Pattern recognition in existing events
- Intelligent time slot suggestions
- Context-aware recommendations

## Customization

### Colors and Themes
The application uses a modern dark theme with:
- Primary color: #2a82da (blue)
- Background: #1e1e1e (dark gray)
- Accent colors for different event types
- Customizable event colors

### Event Categories
Default categories include:
- Study
- Exam
- Assignment
- Meeting
- Break
- Exercise
- Personal
- Other

## Future Enhancements

- Integration with external calendar services
- Cloud synchronization
- Advanced AI features with external APIs
- Mobile companion app
- Team collaboration features
- Export/import functionality

## License

This project is open source and available under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Support

For support or questions, please open an issue in the project repository.
