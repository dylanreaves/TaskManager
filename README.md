# TaskManager

## Author
Dylan Reaves  
CSC 211H Honors Project  
Spring 2026

## Inspiration
I created TaskManager because keeping track of daily responsibilities can become difficult when information is spread across different places. Tasks, reminders, deadlines, and events are often stored in notes, calendars, messages, or memory, which makes it harder to stay organized. I wanted to build a single application that could manage all of those schedule items in one place.

## Project Description
TaskManager is a desktop application built in C++ using Qt. It is designed to help users manage different types of schedule items, including tasks, events, reminders, and schedules. The application allows users to create, edit, delete, complete, search, and filter items through an interactive graphical interface.

The interface includes:
- a navigation and filters section
- a Today’s Agenda section
- a structured event list
- a month calendar
- a details panel for the selected item

The application also saves data locally so that events remain available between sessions.

## Main Features
- Create, edit, delete, and complete schedule items
- Support for multiple item types:
  - Task
  - Event
  - Reminder
  - Schedule
- Store additional details such as:
  - date and time
  - priority
  - category
  - location
  - description
- Search and filter events
- View schedule items in both a list and calendar
- Save data locally to a file

## Algorithm
The application follows a simple workflow:

1. Open the application
2. Load saved event data from local storage
3. Display the main interface
4. Allow the user to:
   - create a new item
   - select an existing item
   - edit or delete an item
   - mark an item as completed
   - search or filter visible items
   - interact with the calendar
5. Update the visible list, calendar, and details panel when changes are made
6. Save updated event data locally
7. Continue allowing the user to manage their schedule

## Challenges
### 1. Moving from command line to GUI
One challenge was turning the original command-line prototype into a full graphical desktop application. The command-line version helped test the logic, but redesigning that into a user-friendly interface required rethinking how the user would interact with the program visually.

### 2. Keeping the interface synchronized
The application includes several connected parts, such as the Today’s Agenda section, the main event list, the month calendar, filters, and the details panel. A challenge was making sure that when an item was created, edited, completed, or deleted, all of those areas updated correctly and stayed consistent.

### 3. Managing different types of schedule data
The application supports multiple kinds of schedule items and each one can contain several details like date, time, category, priority, and description. Designing the logic to manage those different item types cleanly while also saving and loading data locally was another major challenge.

## Accomplishments / What I Learned
### 1. Structuring a larger C++ project
This project helped me learn how to organize a larger C++ program across multiple files and classes instead of keeping everything in one place.

### 2. Building a GUI with Qt
I gained experience using Qt to create a desktop interface, including widgets, layouts, signals and slots, and Qt data types like `QString`, `QDate`, and `QDateTime`.

### 3. Connecting logic to a visual application
One of the biggest accomplishments of this project was turning a working command-line prototype into a complete desktop application with local storage, multiple views, and interactive controls.

## Future Direction
A future improvement for this project would be integrating local AI support so the application could be interacted with using natural language. This could allow users to create, update, or search schedule items through plain text commands instead of only using buttons and forms.

Additional future improvements may include:
- more advanced recurring schedule options
- notifications or reminders
- improved analytics or summaries of scheduled work
- expanded filtering and organization tools

## Built With
- C++
- Qt
- CMake

## How to Build
1. Install Qt and CMake
2. Clone this repository
3. Open the project in Qt Creator or configure it with CMake
4. Build and run the project

Example CMake commands:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64"
cmake --build build
