/*
 * Copyright 2013-2025 SLAMTEC Co., Ltd.
 * Aurora Remote SDK - Dashcam Recorder Demo
 *
 * This demo shows how to use the dashcam recorder APIs to monitor and control
 * the device's dashcam recording functionality. Features an interactive console
 * with auto-refresh display.
 */

#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <string>
#include <atomic>
#include <ctime>
#include <sstream>
#include <vector>
#include <cstring>

// Platform-specific headers
#ifdef _WIN32
    #include <conio.h>
    #define STRNCPY_SAFE(dest, src, size) strncpy_s(dest, size, src, _TRUNCATE)
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #define STRNCPY_SAFE(dest, src, size) do { strncpy(dest, src, (size) - 1); dest[(size) - 1] = '\0'; } while(0)
#endif

// Simple color enum
enum class Color {
    RESET = 0,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    WHITE,
    BOLD
};

#ifndef _WIN32
// ANSI codes for Linux only
const char* ANSI_RESET   = "\033[0m";
const char* ANSI_RED     = "\033[31m";
const char* ANSI_GREEN   = "\033[32m";
const char* ANSI_YELLOW  = "\033[33m";
const char* ANSI_BLUE    = "\033[34m";
const char* ANSI_MAGENTA = "\033[35m";
const char* ANSI_CYAN    = "\033[36m";
const char* ANSI_WHITE   = "\033[37m";
const char* ANSI_BOLD    = "\033[1m";
#endif

#ifdef _WIN32
// Windows - simple implementation without colors

void setColor(Color color) {
    (void)color;
}

void clearScreen() {
    system("cls");
}

void hideCursor() {
}

void showCursor() {
}

struct TerminalMode {
    TerminalMode() {}
    ~TerminalMode() {}
    void setRawMode() {}
    void setNormalMode() {}
};

char getKeyPress() {
    if (_kbhit()) {
        return _getch();
    }
    return 0;
}

#else
// Linux terminal control and color support

void setColor(Color color) {
    switch (color) {
        case Color::RESET:   std::cout << ANSI_RESET; break;
        case Color::RED:     std::cout << ANSI_RED; break;
        case Color::GREEN:   std::cout << ANSI_GREEN; break;
        case Color::YELLOW:  std::cout << ANSI_YELLOW; break;
        case Color::BLUE:    std::cout << ANSI_BLUE; break;
        case Color::MAGENTA: std::cout << ANSI_MAGENTA; break;
        case Color::CYAN:    std::cout << ANSI_CYAN; break;
        case Color::WHITE:   std::cout << ANSI_WHITE; break;
        case Color::BOLD:    std::cout << ANSI_BOLD; break;
        default:             std::cout << ANSI_RESET; break;
    }
}

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

void hideCursor() {
    std::cout << "\033[?25l";
}

void showCursor() {
    std::cout << "\033[?25h";
}

struct TerminalMode {
    struct termios original;
    bool saved = false;

    TerminalMode() {
        tcgetattr(STDIN_FILENO, &original);
        saved = true;
        setRawMode();
    }

    ~TerminalMode() {
        if (saved) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
    }

    void setRawMode() {
        struct termios raw = original;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    void setNormalMode() {
        if (saved) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
    }
};

char getKeyPress() {
    // Use select() to check if input is available (non-blocking)
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    tv.tv_sec = 0;
    tv.tv_usec = 0;  // No wait - return immediately

    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            return c;
        }
    }
    return 0;
}
#endif

std::string workingStateToString(slamtec_aurora_sdk_dashcam_working_state_t state) {
    switch (state) {
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_UNKNOWN: return "UNKNOWN";
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_INITIALIZING: return "INITIALIZING";
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_READY: return "READY";
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_RECORDING: return "RECORDING";
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_ERROR_INIT: return "ERROR_INIT";
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_ERROR_STORAGE_FULL: return "ERROR_STORAGE_FULL";
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_ERROR_WRITE_FAILED: return "ERROR_WRITE_FAILED";
        default: return "UNKNOWN";
    }
}

Color getStateColor(slamtec_aurora_sdk_dashcam_working_state_t state) {
    switch (state) {
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_RECORDING: return Color::GREEN;
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_READY: return Color::CYAN;
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_INITIALIZING: return Color::YELLOW;
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_ERROR_INIT:
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_ERROR_STORAGE_FULL:
        case SLAMTEC_AURORA_SDK_DASHCAM_STATE_ERROR_WRITE_FAILED: return Color::RED;
        default: return Color::WHITE;
    }
}

void printColored(const std::string& text, Color color) {
    setColor(color);
    std::cout << text;
    setColor(Color::RESET);
}

std::string formatTimestamp(uint64_t microseconds) {
    time_t seconds = microseconds / 1000000;
    struct tm timeinfo_storage;
#ifdef _WIN32
    localtime_s(&timeinfo_storage, &seconds);
#else
    localtime_r(&seconds, &timeinfo_storage);
#endif
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo_storage);
    return std::string(buffer);
}

std::string formatDuration(uint64_t startMicros, uint64_t endMicros) {
    if (endMicros <= startMicros) {
        return "0s";
    }

    uint64_t durationSeconds = (endMicros - startMicros) / 1000000;
    uint64_t hours = durationSeconds / 3600;
    uint64_t minutes = (durationSeconds % 3600) / 60;
    uint64_t seconds = durationSeconds % 60;

    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << "h " << minutes << "m";
    } else if (minutes > 0) {
        oss << minutes << "m " << seconds << "s";
    } else {
        oss << seconds << "s";
    }
    return oss.str();
}

std::string formatBytes(uint64_t bytes) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        oss << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    } else if (bytes >= 1024ULL * 1024ULL) {
        oss << (bytes / (1024.0 * 1024.0)) << " MB";
    } else if (bytes >= 1024ULL) {
        oss << (bytes / 1024.0) << " KB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

void printDashboard(slamtec_aurora_sdk_session_handle_t handle) {
    clearScreen();

    // Get current time for header
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo_storage;
#ifdef _WIN32
    localtime_s(&timeinfo_storage, &time_t_now);
#else
    localtime_r(&time_t_now, &timeinfo_storage);
#endif
    char timeBuffer[32];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo_storage);

    // Header
    setColor(Color::BOLD);
    setColor(Color::CYAN);
    std::cout << "================================================================================\n";
    std::cout << "        AURORA SDK - DASHCAM MONITOR & CONTROL              [" << timeBuffer << "]\n";
    std::cout << "================================================================================\n";
    setColor(Color::RESET);
    std::cout << "\n";

    // Get Status
    slamtec_aurora_sdk_dashcam_status_t status;
    memset(&status, 0, sizeof(status));
    auto err = slamtec_aurora_sdk_dashcam_recorder_get_status(handle, &status, 1000);

    // Status Section
    setColor(Color::BOLD);
    std::cout << "# RECORDING STATUS";
    setColor(Color::RESET);
    std::cout << "\n";
    std::cout << "-------------------------------------------------------------------------------\n";

    if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
        std::cout << "  Enabled:           ";
        printColored(status.enabled ? "YES" : "NO", status.enabled ? Color::GREEN : Color::RED);
        std::cout << "\n";

        std::cout << "  Recording:         ";
        printColored(status.recording ? "YES" : "NO", status.recording ? Color::GREEN : Color::RED);
        std::cout << "\n";

        std::cout << "  Working State:     ";
        setColor(getStateColor(status.working_state));
        setColor(Color::BOLD);
        std::cout << workingStateToString(status.working_state);
        setColor(Color::RESET);
        std::cout << "\n";

        std::cout << "  Status Message:    ";
        printColored(status.working_message, Color::YELLOW);
        std::cout << "\n";

        std::cout << "  Size Limit:        " << std::fixed << std::setprecision(2)
                  << status.size_limit_gb << " GB\n";
        std::cout << "  Current Size:      " << formatBytes(status.current_size_bytes) << "\n";
        std::cout << "  Last Update:       " << formatTimestamp(status.working_timestamp) << "\n";
    } else {
        std::cout << "  ";
        printColored("[Failed to get status]", Color::RED);
        std::cout << "\n";
    }

    std::cout << "\n";

    // Storage Section
    setColor(Color::BOLD);
    std::cout << "# STORAGE INFORMATION";
    setColor(Color::RESET);
    std::cout << "\n";
    std::cout << "-------------------------------------------------------------------------------\n";

    // Get storage info using the opaque handle API
    slamtec_aurora_sdk_dashcam_storage_info_t storageInfo = slamtec_aurora_sdk_dashcam_recorder_get_storage_info(handle, 1000);

    if (storageInfo) {
        // Get storage path
        const char* storagePath = slamtec_aurora_sdk_dashcam_storage_info_get_path(storageInfo);
        std::cout << "  Storage Path:      " << (storagePath ? storagePath : "N/A") << "\n";

        // Get storage status if available
        if (slamtec_aurora_sdk_dashcam_storage_info_has_storage_status(storageInfo) > 0) {
            slamtec_aurora_sdk_dashcam_storage_status_t storage;
            memset(&storage, 0, sizeof(storage));
            if (slamtec_aurora_sdk_dashcam_storage_info_get_storage_status(storageInfo, &storage) == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                std::cout << "  External Storage:  ";
                printColored(storage.using_external_storage ? "ENABLED" : "DISABLED",
                             storage.using_external_storage ? Color::CYAN : Color::YELLOW);
                std::cout << "\n";

                if (storage.using_external_storage) {
                    std::cout << "    - Present:       ";
                    printColored(storage.external_storage_present ? "YES" : "NO",
                                 storage.external_storage_present ? Color::GREEN : Color::RED);
                    std::cout << "\n";

                    std::cout << "    - Mounted:       ";
                    printColored(storage.external_storage_mounted ? "YES" : "NO",
                                 storage.external_storage_mounted ? Color::GREEN : Color::RED);
                    std::cout << "\n";
                }

                std::cout << "  Total Space:       " << formatBytes(storage.total_space_bytes) << "\n";
                std::cout << "  Free Space:        " << formatBytes(storage.free_space_bytes);

                if (storage.total_space_bytes > 0) {
                    float freePercent = (storage.free_space_bytes * 100.0f) / storage.total_space_bytes;
                    std::cout << " (" << std::fixed << std::setprecision(1) << freePercent << "%)";
                }
                std::cout << "\n";

                std::cout << "  Used by Dashcam:   " << formatBytes(storage.used_by_dashcam_bytes) << "\n";
                std::cout << "  Last Update:       " << formatTimestamp(storage.last_update_time) << "\n";
            }
        }

        // Sessions
        int sessionCount = slamtec_aurora_sdk_dashcam_storage_info_get_session_count(storageInfo);
        std::cout << "\n  Existing Sessions: " << (sessionCount >= 0 ? std::to_string(sessionCount) : "N/A") << "\n";

        if (sessionCount > 0) {
            std::cout << "  +---------+-------+---------------------+----------+-----------+\n";
            std::cout << "  | Session | Blobs | Start Time          | Duration | Size      |\n";
            std::cout << "  +---------+-------+---------------------+----------+-----------+\n";

            for (int i = 0; i < sessionCount && i < 10; i++) {
                slamtec_aurora_sdk_dashcam_session_info_t sessionInfo;
                memset(&sessionInfo, 0, sizeof(sessionInfo));

                if (slamtec_aurora_sdk_dashcam_storage_info_get_session(storageInfo, i, &sessionInfo) == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                    std::cout << "  | " << std::setw(7) << sessionInfo.session_id << " | "
                              << std::setw(5) << sessionInfo.blob_idx_count << " | "
                              << formatTimestamp(sessionInfo.start_time) << " | "
                              << std::setw(8) << formatDuration(sessionInfo.start_time, sessionInfo.end_time) << " | "
                              << std::setw(9) << formatBytes(sessionInfo.size) << " |\n";
                }
            }

            if (sessionCount > 10) {
                std::cout << "  | ... and " << (sessionCount - 10) << " more sessions                                  |\n";
            }
            std::cout << "  +---------+-------+---------------------+----------+-----------+\n";
        }

        // Current session
        if (slamtec_aurora_sdk_dashcam_storage_info_has_current_session(storageInfo) > 0) {
            slamtec_aurora_sdk_dashcam_session_info_t currentSession;
            memset(&currentSession, 0, sizeof(currentSession));

            if (slamtec_aurora_sdk_dashcam_storage_info_get_current_session(storageInfo, &currentSession) == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                std::cout << "\n  ";
                setColor(Color::BOLD);
                setColor(Color::GREEN);
                std::cout << "CURRENT SESSION:";
                setColor(Color::RESET);
                std::cout << " Session #" << currentSession.session_id
                          << " | Blobs: " << currentSession.blob_idx_count
                          << " | Size: " << formatBytes(currentSession.size) << "\n";
            }
        }

        // Destroy the storage info handle when done
        slamtec_aurora_sdk_dashcam_storage_info_destroy(storageInfo);
    } else {
        std::cout << "  ";
        printColored("[Failed to get storage info]", Color::RED);
        std::cout << "\n";
    }

    std::cout << "\n";

    // Controls
    setColor(Color::BOLD);
    std::cout << "# CONTROLS";
    setColor(Color::RESET);
    std::cout << "\n";
    std::cout << "-------------------------------------------------------------------------------\n";
    std::cout << "  ";
    printColored("[S]", Color::CYAN);
    std::cout << " Start (Enable)    ";
    printColored("[T]", Color::CYAN);
    std::cout << " Stop (Disable)    ";
    printColored("[L]", Color::CYAN);
    std::cout << " Set Size Limit\n";
    std::cout << "  ";
    printColored("[I]", Color::CYAN);
    std::cout << " Invalidate Sessions         ";
    printColored("[R]", Color::CYAN);
    std::cout << " Refresh    ";
    printColored("[Q]", Color::CYAN);
    std::cout << " Quit\n";
    std::cout << "\n";

    std::cout.flush();
}

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " <server_address>\n";
    std::cout << "Example: " << progName << " 192.168.11.1\n";
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const char* serverAddr = argv[1];

    std::cout << "Connecting to Aurora server at " << serverAddr << "...\n";

    // Create SDK session using C API
    slamtec_aurora_sdk_errorcode_t err;
    auto sessionHandle = slamtec_aurora_sdk_create_session(nullptr, 0, nullptr, &err);
    if (!sessionHandle || err != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
        printColored("Failed to create SDK session\n", Color::RED);
        return 1;
    }

    // Create connection info
    slamtec_aurora_sdk_server_connection_info_t connInfo;
    memset(&connInfo, 0, sizeof(connInfo));
    connInfo.connection_count = 1;
    STRNCPY_SAFE(connInfo.connection_info[0].address, serverAddr, sizeof(connInfo.connection_info[0].address));
    connInfo.connection_info[0].port = SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_PORT;
    STRNCPY_SAFE(connInfo.connection_info[0].protocol_type, SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_PROTOCOL, sizeof(connInfo.connection_info[0].protocol_type));

    // Connect to server
    err = slamtec_aurora_sdk_controller_connect(sessionHandle, &connInfo);
    if (err != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
        printColored("Failed to connect to server\n", Color::RED);
        slamtec_aurora_sdk_release_session(sessionHandle);
        return 1;
    }

    std::cout << "Connected successfully!\n";
    std::cout << "Dashcam recorder ready!\n";
    std::cout << "Starting interactive monitor...\n\n";

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Setup terminal mode
    TerminalMode termMode;
    hideCursor();

    bool running = true;
    const auto refreshInterval = std::chrono::seconds(2);

    // Initial display
    printDashboard(sessionHandle);
    auto lastRefresh = std::chrono::steady_clock::now();

    while (running) {
        // Auto-refresh every 2 seconds
        auto now = std::chrono::steady_clock::now();
        if (now - lastRefresh >= refreshInterval) {
            printDashboard(sessionHandle);
            lastRefresh = std::chrono::steady_clock::now();
        }

        // Check for key press
        char key = getKeyPress();
        if (key != 0) {
            // Convert to lowercase
            if (key >= 'A' && key <= 'Z') {
                key = key + ('a' - 'A');
            }

            switch (key) {
                case 's': {
                    // Start (enable)
                    clearScreen();
                    std::cout << "Enabling dashcam recorder...\n";
                    auto err = slamtec_aurora_sdk_dashcam_recorder_set_enable(sessionHandle, 1, 5000);
                    if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                        printColored("SUCCESS: Dashcam recorder enabled\n", Color::GREEN);
                    } else {
                        printColored("FAILED: Could not enable dashcam recorder\n", Color::RED);
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    printDashboard(sessionHandle);
                    lastRefresh = std::chrono::steady_clock::now();
                    break;
                }

                case 't': {
                    // Stop (disable)
                    clearScreen();
                    std::cout << "Disabling dashcam recorder...\n";
                    auto err = slamtec_aurora_sdk_dashcam_recorder_set_enable(sessionHandle, 0, 5000);
                    if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                        printColored("SUCCESS: Dashcam recorder disabled\n", Color::GREEN);
                    } else {
                        printColored("FAILED: Could not disable dashcam recorder\n", Color::RED);
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    printDashboard(sessionHandle);
                    lastRefresh = std::chrono::steady_clock::now();
                    break;
                }

                case 'l': {
                    // Set size limit - switch to normal terminal mode for input
                    termMode.setNormalMode();
                    showCursor();
                    clearScreen();
                    std::cout << "Enter new size limit in GB (e.g., 10.0): ";
                    std::cout.flush();

                    float newLimit = 0.0f;
                    std::cin >> newLimit;

                    // Clear input buffer
                    std::cin.clear();
                    std::cin.ignore(10000, '\n');

                    // Switch back to raw mode
                    hideCursor();
                    termMode.setRawMode();

                    if (newLimit > 0) {
                        std::cout << "Setting size limit to " << std::fixed << std::setprecision(2) << newLimit << " GB...\n";
                        auto err = slamtec_aurora_sdk_dashcam_recorder_set_size_limit(sessionHandle, newLimit, 5000);
                        if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                            printColored("SUCCESS", Color::GREEN);
                            std::cout << ": Size limit set to " << std::fixed << std::setprecision(2) << newLimit << " GB\n";
                        } else {
                            printColored("FAILED", Color::RED);
                            std::cout << ": Could not set size limit (error code: " << err << ")\n";
                        }
                    } else {
                        printColored("CANCELLED: Invalid size limit (must be > 0)\n", Color::YELLOW);
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    printDashboard(sessionHandle);
                    lastRefresh = std::chrono::steady_clock::now();
                    break;
                }

                case 'i': {
                    // Invalidate sessions
                    clearScreen();
                    std::cout << "Invalidating all sessions...\n";
                    auto err = slamtec_aurora_sdk_dashcam_recorder_invalidate_sessions(sessionHandle, 10000);
                    if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                        printColored("SUCCESS: All sessions invalidated\n", Color::GREEN);
                    } else {
                        printColored("FAILED: Could not invalidate sessions\n", Color::RED);
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    printDashboard(sessionHandle);
                    lastRefresh = std::chrono::steady_clock::now();
                    break;
                }

                case 'r': {
                    // Manual refresh
                    printDashboard(sessionHandle);
                    lastRefresh = std::chrono::steady_clock::now();
                    break;
                }

                case 'q': {
                    // Quit
                    running = false;
                    break;
                }
            }
        }

        // Small sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    showCursor();
    clearScreen();

    std::cout << "Cleaning up...\n";
    slamtec_aurora_sdk_release_session(sessionHandle);

    std::cout << "Goodbye!\n";

    return 0;
}
