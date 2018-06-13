
//linker.cpp
//Links gpsData.txt and RFID data(data.txt)
//Date: 3/12/18
//Jesse Batstone


#include <iostream>
#include <fstream>
#include <string.h>

#include <stdio.h>
#include <string>
#include <limits>
#include <sstream>
#include <vector>


using namespace std;

int SIZE = 10000;
int WORDS = 4;



int main()
{
	//OPEN FILES
    FILE * output;
    output=fopen("linked.txt", "w");
    string gpsFile, rfidFile;
    ifstream gpsData, rfidData;

    gpsFile="gpsData.txt" ;
    rfidFile="rfidData.txt" ;

    gpsData.open( gpsFile.c_str(), ios::binary );
    rfidData.open( rfidFile.c_str(), ios::binary );

    if (!gpsData)
    {cout << "Couldn't open the file  " << gpsFile<<endl;
    return 1;
    }

    if (!rfidData)
    {cout << "Couldn't open the file " << rfidFile << endl;
    return 1;
    }

	//PARSE DATA

	int gpsSize=0;
	int rfidSize=0;
	//Break up gpsData.txt into Tokens
	string gpsInfo[SIZE][WORDS];
	string line;
	for (int lineNum = 0; getline(gpsData, line); lineNum++)
	{
    		stringstream ss(line);
    		for (int wordNum = 0; ss >> gpsInfo[lineNum][wordNum]; wordNum++)
    		{
       			//cout << " " << wordNum << "." << gpsInfo[lineNum][wordNum];
    		}
    		//cout << endl;
		gpsSize++;
	}

	//Break up rfid Data(data.txt) into tokens
	string rfidInfo[SIZE][WORDS];
        string line1;
        for (int lineNum = 1; getline(rfidData, line1); lineNum++)
        {
                stringstream ss(line1);
		int wordNum = 0;
                for (int wordNum = 0; ss >> rfidInfo[lineNum][wordNum]; wordNum++)
		{
                        //cout << " " << wordNum << "." << rfidInfo[lineNum][wordNum];
                }
		//cout << endl;
		rfidSize++;
        }

	//COMPARE RFID TIME TO GPSTIME AND STORE INDEX OF MATCH
	string rfidTime, rfidTime1, rfidTime2; //formated rfid time
	int rangeUp, rangeDown;		//range time +-1
	int matches = 0;		//matches index
	int matchMatrix[SIZE][2]; //stores matches
	for(int i=0; i<rfidSize; i++){
		rfidTime = rfidInfo[i][3][16];
		rfidTime +=rfidInfo[i][3][17];
		rfidTime +=rfidInfo[i][3][19];
		rfidTime +=rfidInfo[i][3][20];
		rfidTime +=rfidInfo[i][3][22];
		rfidTime1 = rfidTime2 =rfidTime;
		rfidTime +=rfidInfo[i][3][23];

		rangeUp = (int)rfidInfo[i][3][23]+1;
		rangeDown = (int)rfidInfo[i][3][23]-1;
		rfidTime1 += (char)rangeUp;
		rfidTime2 += (char)rangeDown;
		//cout << i << ": "<<rfidTime<<" "<<rfidTime1 << " " << rfidTime2 << endl;
		for(int j=0; j<gpsSize; j++){
			if(gpsInfo[j][0].compare(rfidTime) ==0 ||
			gpsInfo[j][0].compare(rfidTime1) ==0 ||
			gpsInfo[j][0].compare(rfidTime2) ==0){
				//cout<<"Match: rfid: "<< i<< " gps: "<< j << endl;
				matchMatrix[matches][0] = i;
				matchMatrix[matches][1] = j;
				matches++;
			}

		}

	}
	//Store EPC with corresponding GPS location
	printf("|--------------------------------------------|\n");
        printf("| %6s | %6s | %10s | %10s |\n", " Time ", " Tags ", "  Latitude ", " Longitude");
	printf("|--------------------------------------------|\n");
	fflush(stdout);
        fprintf(output, "|--------------------------------------------------------------------------------|\n");
        fprintf(output, "| %6s | %42s | %10s | %10s |\n", " Time ", "Tags                 ", "  Latitude ", " Longitude");
        fprintf(output, "|--------------------------------------------------------------------------------|\n");

	for(int i=0; i<matches; i++){
		if(rfidInfo[matchMatrix[i][0]][0].length()>38){
			//printf("| %6s | %6s | %10s | %10s |\n",gpsInfo[matchMatrix[i][1]][0].c_str(),rfidInfo[matchMatrix[i][0]][0].substr(32,6).c_str(),gpsInfo[matchMatrix[i][1]][2].c_str(),gpsInfo[matchMatrix[i][1]][3].c_str());
			//fflush(stdout);
			fprintf(output, "| %6s | %42s | %10s | %10s |\n",gpsInfo[matchMatrix[i][1]][0].c_str(),rfidInfo[matchMatrix[i][0]][0].substr(0,42).c_str(), gpsInfo[matchMatrix[i][1]][2].c_str(),gpsInfo[matchMatrix[i][1]][3].c_str());
		}

	}


	//UNIQUE EPC CODES
	string uniqueEPC[rfidSize];
	bool match = 0;
	int uniqueIndex = 0;
	for(int i=0; i<matches; i++){
		for(int j=0; j<uniqueIndex; j++){
			if(rfidInfo[matchMatrix[i][0]][0].substr(0,38)==uniqueEPC[j]){
				match = 1;
				break;
			} 
		}
		if(match == 0){
			uniqueEPC[uniqueIndex]=rfidInfo[matchMatrix[i][0]][0].substr(0,38);
			printf("| %6s | %6s | %10s | %10s |\n",gpsInfo[matchMatrix[i][1]][0].c_str(),rfidInfo[matchMatrix[i][0]][0].substr(32,6).c_str(),gpsInfo[matchMatrix[i][1]][2].c_str(),gpsInfo[matchMatrix[i][1]][3].c_str());
			uniqueIndex++;
		}
		match = 0;
	}
	fclose(output);
	printf("|--------------------------------------------|\n");
	cout << "\nUniqueEPCs: " << uniqueIndex<<endl<< endl;
	cout << "Open linked.txt for more data\n\n";
    return 0;
}



