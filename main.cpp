#include "dmf2esf.h"

using namespace std;

bool OutputInstruments = false;
bool ASMOut = false;
bool ExCommands = false;

int main(int argc, char *argv[])
{
    int     InputId = 0;
    int     OutputId = 0;
    int     Opt = 0;

    OutputInstruments = false;
    ExCommands = false;
	bool Verbose = false;

    DMFConverter * dmf;
    ESFOutput * esf;
    INIReader * ini;

    fprintf(stdout, "\nDMF2ESF ver %d.%d (built %s %s)\n", MAJORVER, MINORVER, __DATE__, __TIME__);
    fprintf(stdout, "Copyright 2013-2014 ctr.\n");
	fprintf(stdout, "Extended by Big Evil Corporation 2017.\n");
    fprintf(stdout, "Licensed under GPLv2, see LICENSE.txt.\n\n");
	fprintf(stdout, "Includes source from Secret Rabbit Code, licensed under GPLv1, see libsamplerate\\COPYING.\n\n");

	int instrumentIdxOffset = 0;
	bool error = false;
	std::vector<std::pair<std::string, std::string>> filenames;
	const char* configFile = NULL;

    if(argc > 1)
    {
		for(int i = 1; i < argc; ++i)
		{
			/* Check for options */
			if(!strcmp(argv[i], "-i"))
			{
				OutputInstruments = true;
			}
			else if(!strcmp(argv[i], "-a"))
			{
				ASMOut = true;
			}
			else if(!strcmp(argv[i], "-e"))
			{
				ExCommands = true;
			}
			else if(!strcmp(argv[i], "-v"))
			{
				Verbose = true;
			}
			else if(!strcmp(argv[i], "-c"))
			{
				i++;
				if(i < argc)
				{
					configFile = argv[i];
				}
				else
				{
					error = true;
				}
			}
			else if(!strcmp(argv[i], "-instroffset"))
			{
				i++;
				if(i < argc)
				{
					instrumentIdxOffset = atoi(argv[i]);
				}
				else
				{
					error = true;
				}
			}
			else
			{
				//Filename
				std::string inFile = argv[i++];

				if(i < argc)
				{
					std::string outFile = argv[i];
					filenames.push_back(std::make_pair(inFile, outFile));
				}
				else
				{
					error = true;
				}
			}
		}
    }

	if(error || filenames.size() == 0) // no input file?
    {
        //print usage and exit
        fprintf(stderr, "Usage: dmf2esf <options> input output <input output> <input output>...\n");
        fprintf(stderr, "Options:\n");
		fprintf(stderr, "\t-c <config.ini> : Use INI config file\n");
        fprintf(stderr, "\t-i : Output FM instrument data\n");
        fprintf(stderr, "\t-a : Output ESF as an assembly file\n");
        fprintf(stderr, "\t-e : Use EchoEx extended commands\n");
		fprintf(stderr, "\t-v : Verbose output\n");
		fprintf(stderr, "\t-instroffset : Offset the first instrument index\n");
        fprintf(stderr, "Please read \"readme.md\" for further usage instructions.\n");

        for(int a=0;a<12*7;a++)
        {
            fprintf(stderr, "%d\n", fmfreq(a));
        }
    }
    else
    {
        if(configFile)
        {
            fprintf(stdout, "Loading ini: %s\n",configFile);
            ini = new INIReader(configFile);

            if (ini->ParseError() < 0) {
                fprintf(stderr, "Failed to load ini: %s\n",argv[InputId]);
                return EXIT_FAILURE;
            }

            bool SearchFlag=true;
            int TrackIndex=0;
            char IniSection[64];
            string input;

            while(SearchFlag)
            {
                sprintf(IniSection,"%d",TrackIndex);
                fprintf(stdout, "======== Reading [%s] ========\n",IniSection);
                string input = ini->Get(IniSection,"input","");
                if(input.length() > 0)
                {
                    if(input.rfind(".") < 0)
                    {
                        fprintf(stderr, "Silly input\n");
                        return EXIT_FAILURE;
                    }

                    string output = ini->Get(IniSection,"output","");
                    bool TrackSFX = ini->GetBoolean(IniSection,"sfx",false);
                    if(output.length() == 0)
                        output = input.substr(0,input.rfind("."));

                    if(output.rfind(".") < 0)
                    {
                        fprintf(stderr, "Silly output\n");
                        return EXIT_FAILURE;
                    }

                    fprintf(stderr, "Converting %s to %s\n",input.c_str(), output.c_str());

                    esf = new ESFOutput(output);
                    dmf = new DMFConverter(&esf);

                    FindInstruments(IniSection,ini,dmf);

                    if(dmf->Initialize(input.c_str()))
                    {
                        fprintf(stderr, "Failed to initialize, aborting\n");
                        break;
                    }
                    if(dmf->Parse())
                    {
                        fprintf(stderr, "Conversion failed, aborting\n");
                        break;
                    }
                    fprintf(stdout, "Successfully converted, continuing.\n");
                    delete esf;
                    delete dmf;
                }
                else
                {
                    //fprintf(stderr, "invalid input '%s' (%d)\n",input.c_str(), input.length());
                    break;
                }
                TrackIndex++;
            }
            delete ini;
        }
        else
        {
			for(int i = 0; i < filenames.size(); i++)
			{
				fprintf(stdout, "Converting: %s\n", filenames[i].first.c_str());

				esf = new ESFOutput(filenames[i].second);
				dmf = new DMFConverter(&esf);

				esf->VerboseLog = Verbose;
				dmf->VerboseLog = Verbose;

				esf->InstrumentOffset = instrumentIdxOffset;
				dmf->InstrumentOffset = instrumentIdxOffset;

				if(dmf->Initialize(filenames[i].first.c_str()))
				{
					fprintf(stderr, "Aborting\n");
					return EXIT_FAILURE;
				}
				if(dmf->Parse())
				{
					fprintf(stderr, "Conversion aborted.\n");
					return EXIT_FAILURE;
				}

				instrumentIdxOffset += dmf->TotalInstruments;

				delete dmf;
				delete esf;
			}
        }
    }
    return EXIT_SUCCESS;
}
