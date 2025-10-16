# EduSync Demo Guide

## Getting Started

1. **Build the Application**
   ```bash
   ./build.sh
   ```

2. **Run EduSync**
   ```bash
   cd build
   ./EduSync
   ```

## Demo Features

### 1. Modern Interface
- **Dark Theme**: Beautiful dark interface with blue accents
- **Split Panels**: Calendar on the left, AI analysis on the right
- **Responsive Layout**: Resizable panels for optimal viewing

### 2. Calendar Navigation
- **Month Navigation**: Use ◀ ▶ buttons or keyboard shortcuts
- **Today Button**: Quick return to current date
- **Date Selection**: Click any date to view events

### 3. Event Management
- **Add Event**: Click "Add Event" button
  - Enter title: "Math Study Session"
  - Description: "Review calculus concepts"
  - Set time: 2:00 PM - 4:00 PM
  - Choose color: Blue
  - Category: Study

- **Edit Event**: Double-click any event in the list
- **Delete Event**: Select event and click "Delete Selected"

### 4. AI Features Demo

#### AI Suggestions
1. Add a few events to your calendar
2. Click "✨ Get AI Suggestions"
3. See AI-generated recommendations based on your schedule

#### Schedule Analysis
1. The AI panel automatically analyzes your schedule
2. Click "🔄 Refresh" to update analysis
3. View insights about your calendar patterns

### 5. Sample Schedule Setup

Try creating this sample schedule to see AI in action:

**Monday:**
- 9:00 AM - 11:00 AM: "Physics Lecture" (Blue, Study)
- 2:00 PM - 4:00 PM: "Math Homework" (Green, Assignment)

**Tuesday:**
- 10:00 AM - 12:00 PM: "Chemistry Lab" (Purple, Study)
- 3:00 PM - 5:00 PM: "History Reading" (Orange, Study)

**Wednesday:**
- 1:00 PM - 3:00 PM: "Math Exam" (Red, Exam)

After adding these events, click "Get AI Suggestions" to see how the AI recommends:
- Study preparation for the Math Exam
- Break time suggestions
- Exercise recommendations
- Review sessions

### 6. AI Analysis Features

The AI panel will show:
- **Event Distribution**: Count of different event types
- **Recommendations**: Smart suggestions for your schedule
- **Balance Analysis**: Study vs. break time ratio
- **Upcoming Alerts**: Important dates and deadlines

### 7. Advanced Features

#### Color Coding
- Use different colors for different types of events
- Visual organization of your schedule
- Easy identification of event categories

#### Categories
- Study: Academic work
- Exam: Tests and assessments
- Assignment: Homework and projects
- Meeting: Group work or appointments
- Break: Rest and relaxation
- Exercise: Physical activity
- Personal: Individual activities

#### Time Management
- Duration display for each event
- Conflict detection
- Smart time slot suggestions

## Tips for Best Experience

1. **Use Categories**: Assign appropriate categories to events
2. **Color Code**: Use consistent colors for similar events
3. **Regular Updates**: Keep your calendar current
4. **AI Feedback**: Check the AI analysis regularly
5. **Plan Ahead**: Use AI suggestions for better scheduling

## Troubleshooting

### Build Issues
- Ensure Qt 6 is properly installed
- Check CMake version (3.16+)
- Verify C++17 compiler support

### Runtime Issues
- Check file permissions for data storage
- Ensure all Qt libraries are available
- Restart application if needed

## Data Storage

- Events are automatically saved to JSON files
- Data location: `~/.local/share/EduSync/events.json` (Linux/macOS)
- No internet connection required for core functionality
- AI features work locally without external APIs

## Next Steps

1. **Customize**: Adjust colors and categories to your preference
2. **Integrate**: Use alongside other calendar applications
3. **Plan**: Let AI help optimize your schedule
4. **Analyze**: Review AI insights regularly
5. **Improve**: Use suggestions to enhance productivity

Enjoy your AI-powered calendar experience! 🎓✨
