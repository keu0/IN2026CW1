#pragma once
#include <string>

class HighScore {
public:
	std::string name;
	int score;

	HighScore(std::string n, int s) {
		name = n;
		score = s;
	}
};
