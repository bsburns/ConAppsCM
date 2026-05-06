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
#include <conio.h> // For _kbhit() and _getch()

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


void CliMenuProcessor::GetUserInput_thread() {

    // Set up command line processor

	auto& CMP = CliMenuProcessor::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();
	ThreadManager& TM = ThreadManager::GetInstance();
	

    while (!TM.force_stop) {
		if (_kbhit()) {
			unsigned char ch = _getch(); // Get the character without waiting. Returns EOF on failure
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
				//tty_char(&ch, 1); // Process the character through the TTY system
				CMP.ProcessChar(ch);
				watchdog.CheckIn();
			}
		}
		_sleep(200); // Sleep for 200ms to avoid busy loop
    }
}

void stooges() { std::cout << "\n\nHey Moe, it dont woik. NYUK NYUK NYUK NYUK *bop* Owww!\n";}
