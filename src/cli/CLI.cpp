/*------------------------------------------------------------------
 * CLI Source File
 *
 * Defines function for accepting user input from the command line interface (CLI) and executing commands based on that input.
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <iostream>
#include <string>
#include <atomic>
#include <cstdio>

#ifdef _WIN32
#include <conio.h> // For _kbhit() and _getch()
#elif __linux__
#endif

#include "CLI.h"
#include "CliMenu.h"
#include "logger.h"
#include "threadManager.h"

 /*------------------------------------------------------------------------
  * moveCursor()
  *   Called to move the cursor on the current line.
  *   delta - number of positions to move the cursor, (negative moves to the
			  left).
  * Returns new cursor position relative to the beginning of the line.
  *-----------------------------------------------------------------------*/
void CliMenuProcessor::moveCursor(int delta) {
	if (delta < 0) { // Move cursor to left
		delta *= -1; // make it positive
		if (currentCursorPosition > delta) { //room to move
			currentCursorPosition -= delta;
			while (delta--) {
				std::putchar(8); // Backspace
			}
		}
	}
	else { //Move cursor to right
		int cmdLen = (int)commandBuffer.size();

		if (currentCursorPosition + delta > cmdLen) {
			delta = cmdLen - currentCursorPosition;
		}
		while (delta--) {
			std::putchar(commandBuffer[currentCursorPosition++]);
		}
	}
}

void CliMenuProcessor::processEscSeq(char ch) {
	bool rc_continue = true;

	if (ch == 0x41 || ch == 0x42) {
		inEscSeq = false;
		if (ch == 0x41) { // UP Arrow
			itCmdHistory++;
			if (itCmdHistory == commandHistory.end()) itCmdHistory = commandHistory.begin();
		}
		else if (ch == 0x42) { // Down Arrow
			itCmdHistory--;
			if (itCmdHistory == commandHistory.begin()) {
				itCmdHistory = commandHistory.end();
				itCmdHistory--;
			}
		}

		// Clear current line
		int lineSize = (int)commandBuffer.size();
		moveCursor(lineSize);
		moveCursor(-lineSize);

		// Place command from history into current line
		commandBuffer = *itCmdHistory;
		for (char c : commandBuffer) {
			std::putchar(c);
		}
		currentCursorPosition = (int)commandBuffer.size();
	}
	else if (ch == 0x44) { // Left Arrow 
		inEscSeq = false;
		moveCursor(-1);
	}
	else if (ch == 0x43) { // Right Arrow 
		inEscSeq = false;
		moveCursor(1);
	}
	else {
		inEscSeq = false;
		std::cerr << "\nERROR: Unhandled escape sequence: " << ch << "\n";
	}
}

void CliMenuProcessor::updateCommandHistory(const std::string& command) {
	// See if command is already in history, if so move it to the end of the list
	for (auto it = commandHistory.begin(); it != commandHistory.end(); ++it) {
		if (*it == command) {
			commandHistory.erase(it);
			break;
		}
	}
	if (commandHistory.size() >= MAX_COMMAND_HISTORY) {
		// Reached Max command history length
		commandHistory.pop_front();
	}
	commandHistory.push_back(command);
	itCmdHistory = commandHistory.end();
	itCmdHistory--;
}

/*------------------------------------------------------------------------
  * ProcessChar()
  *   Called to process a character input from the user.
  *   ch - character input from the user.
  *-----------------------------------------------------------------------*/
void CliMenuProcessor::ProcessChar(unsigned char ch) {
	if (inEscSeq) { // In an escape sequence
		processEscSeq(ch);
		return;
	}
	switch (ch) {
	case ';':
	case 10:
	case 13:
	case 0: // return key
		if (commandBuffer.empty()) {
			// Nothing in command buffer
			if (prompt.size() > 0) std::cout << prompt;
			return;
		}
		else if (commandBuffer.starts_with("!")) {
			commandBuffer = commandHistory.back();
		}
		else if (commandBuffer.starts_with("?")) {
			rootMenu.Help();
			commandBuffer.clear();
			currentCursorPosition = 0;
			if (prompt.size() > 0) std::cout << "\n" << prompt;
			return;
		}
		rootMenu.ProcessCommand(commandBuffer);

		updateCommandHistory(commandBuffer);
		commandBuffer.clear();
		currentCursorPosition = 0;

		if (prompt.size() > 0) std::cout << "\n" << prompt;
		break;
	case 8: // Backspace
		if (commandBuffer.size() == 0) break;
		if (currentCursorPosition >= commandBuffer.size()) {
			commandBuffer.pop_back(); // Removes last character
			moveCursor(-1);
		}
		else { // removing from middle of string 
			commandBuffer.erase(currentCursorPosition - 1, 1);
			moveCursor(-1);
		}
	case 128: // Delete
		if (commandBuffer.size() == 0) break;
		if (currentCursorPosition >= commandBuffer.size()) break;
		commandBuffer.erase(currentCursorPosition, 1);
		std::cout << commandBuffer.substr(currentCursorPosition) << " ";
		break;
	case 16: // Ctrl-P
	case 2:  // Ctrl-B
		commandBuffer.clear();
		currentCursorPosition = 0;
		break;
	case 27: // ESC
		inEscSeq = true;
		break;
	case 3:  // Ctrl-C
		std::cerr << "\n\nControl-C not implemented yet\n\n";
		break;
	case 31: // Ctrl-U : Kill whole line 
		if (commandBuffer.size() == 0) break;
		for (char ch : commandBuffer) std::putchar(8);
		commandBuffer.clear();
		currentCursorPosition = 0;
		break;
	default:
		if (ch < ' ') break; // bogus ctrl character
		if (ch > 127) break; // ignore high values
		if (ch == ' ' && currentCursorPosition == 0) break; // Ignore leading spaces

		// Normal character
		if (commandBuffer.size() == currentCursorPosition) { // end of command buffer
			commandBuffer.push_back(ch);
			currentCursorPosition++;
		}
		else {
			commandBuffer.insert(currentCursorPosition, 1, ch);
			currentCursorPosition++;
		}
		break;
	}
};


#ifdef _WIN32

void CliMenuProcessor::GetUserInput_thread() {

    // Set up command line processor

	auto& CMP = CliMenuProcessor::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();
	ThreadManager& TM = ThreadManager::GetInstance();

    while (!TM.force_stop) {
		unsigned char ch = std::getchar(); // Get the character without waiting. Returns EOF on failure
		if (ch == EOF) {
			if (std::cin.eof()) {
				std::cout << "End of input detected. Exiting...\n";
				TM.StopAllThreads();
				break;
			}
			else {
				std::cerr << "Error reading input.\n";
			}
		}
		else {
			CMP.ProcessChar(ch);
			watchdog.CheckIn();
		}
    }
}

#elif __linux__

#define ngetc(c) (read (0, (c), 1))
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#if 0
void CliMenuProcessor::GetUserInput_thread() {
    // Set up command line processor
	auto& CMP = CliMenuProcessor::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();
	ThreadManager& TM = ThreadManager::GetInstance();

	// setup Termios for non-blocking read of single characters from STDIN
	struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // Save old settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Disable buffering and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    // Set STDIN to non-blocking mode
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); 

    while (!TM.force_stop) {
		unsigned char ch;
		if (read(STDIN_FILENO, &ch, 1) > 0) { // Read a single character. Returns number of bytes read, or -1 on error
			if (ch == EOF) {
				if (std::cin.eof()) {
					std::cout << "End of input detected. Exiting...\n";
					TM.StopAllThreads();
					break;
				} else {
					std::cerr << "Error reading input.\n";
				}
			} else {
				//tty_char(&ch, 1); // Process the character through the TTY system
				// std::cout << "\nRead character: " << ch << " (" << (int)ch << ")\n"; // Debug output
				std::cout << ch << std::flush; // Ensure character is printed before processing
				CMP.ProcessChar(ch);
				watchdog.CheckIn();
			}
		} else {
			// std::cout << "\nsleep waiting for input...\n";
			std::this_thread::sleep_for(std::ch	struct timeval timeout = {3, 0}; // 3 second timeout
rono::milliseconds(200)); // Sleep for 200ms to avoid busy loop
		}
    }

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore settings
}
#elseif 0
#include <sys/select.h>
void CliMenuProcessor::GetUserInput_thread() {
    // Set up command line processor
	auto& CMP = CliMenuProcessor::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();
	ThreadManager& TM = ThreadManager::GetInstance();

	struct timeval timeout = {3, 0}; // 3 second timeout
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
    
    while (!TM.force_stop) {
		int ret = select(1, &fds, NULL, NULL, &timeout);
		if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
			unsigned char ch = getchar();
			if (ch == EOF) {
				if (std::cin.eof()) {
					std::cout << "End of input detected. Exiting...\n";
					TM.StopAllThreads();
					break;
				} else {
					std::cerr << "Error reading input.\n";
				}
			} else {
				//tty_char(&ch, 1); // Process the character through the TTY system
				// std::cout << "\nRead character: " << ch << " (" << (int)ch << ")\n"; // Debug output
				std::cout << ch << std::flush; // Ensure character is printed before processing
				CMP.ProcessChar(ch);
				watchdog.CheckIn();
			}
		}
	}
}
#else
// Normal blocking read of characters. This is simpler but may not work well in all environments, especially if the input is buffered until a newline is entered.
void CliMenuProcessor::GetUserInput_thread() {
    // Set up command line processor
	auto& CMP = CliMenuProcessor::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();
	ThreadManager& TM = ThreadManager::GetInstance();

    while (!TM.force_stop) {
		unsigned char ch = getchar();
		if (ch == EOF) {
			if (std::cin.eof()) {
				std::cout << "End of input detected. Exiting...\n";
				TM.StopAllThreads();
				break;
			} else {
				std::cerr << "Error reading input.\n";
			}
		} else {
			//tty_char(&ch, 1); // Process the character through the TTY system
			// std::cout << "\nRead character: " << ch << " (" << (int)ch << ")\n"; // Debug output
			std::cout << ch << std::flush; // Ensure character is printed before processing
			CMP.ProcessChar(ch);
			watchdog.CheckIn();
		}
	}
}

#endif  // Different versions of GetUserInput_thread for testing. The one above is the most robust and handles edge cases better, but the one below is simpler and may work better in some environments.
#endif // End of platform-specific code

void stooges() { std::cout << "\n\nHey Moe, it dont woik. NYUK NYUK NYUK NYUK *bop* Owww!\n";}
