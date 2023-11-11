#ifndef EUCLID_H_
#define EUCLID_H_

#include <Arduino.h>
#include <avr/pgmspace.h>




#define NUMBER_OF_STEPS 32

class euclid {

public:

	void generateSequence(uint8_t fills, uint8_t steps );
	void generateRandomSequence(uint8_t fills, uint8_t steps);
	void rotate(uint8_t _steps);
	bool getStep(uint8_t _step);
	uint8_t getStepNumber(){return stepCounter;};
	bool getCurrentStep();
	void doStep();

	void resetSequence();

private:

	bool euclidianPattern[NUMBER_OF_STEPS+1];
	uint8_t stepCounter;
	uint8_t numberOfSteps;
	uint8_t numberOfFills;

};



#endif
