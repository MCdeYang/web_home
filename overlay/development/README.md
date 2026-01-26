# Weather and Task Management Daemon

This project is a multi-threaded application that integrates weather data fetching and task checking functionalities. It is designed to run as a daemon, periodically updating weather information and performing task checks concurrently.

## Project Structure

```
development
├── src
│   ├── main.c            # Entry point of the application
│   ├── environment.c     # Functions for fetching and processing weather data
│   └── check_task.c      # Functions for task checking logic
├── includes
│   ├── environment.h     # Header file for environment.c
│   └── check_task.h      # Header file for check_task.c
├── Makefile              # Build instructions for the project
└── README.md             # Documentation for the project
```

## Setup Instructions

1. **Clone the repository** (if applicable):
   ```bash
   git clone <repository-url>
   cd development
   ```

2. **Build the project**:
   Use the provided `Makefile` to compile the project. Run the following command in the terminal:
   ```bash
   make
   ```

3. **Run the application**:
   After building, you can run the application using:
   ```bash
   ./your_executable_name
   ```

## Usage

- The application will start fetching weather data for Tonghua City and checking tasks concurrently.
- Weather data will be updated every 10 minutes.
- Task checking logic can be customized in `check_task.c`.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue for any enhancements or bug fixes.

## License

This project is licensed under the MIT License - see the LICENSE file for details.