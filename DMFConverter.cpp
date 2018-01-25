#include "dmf2esf.h"
#include "samplerate.h"

#include <set>

template <typename T> T Clamp(T value, T min, T max)
{
	T clamped = value;

	if(value < min)
		clamped = min;
	else if(value > max)
		clamped = max;

	return clamped;
}

using namespace std;

const int DMFFile::sSampleRates[6] =
{
	0,		// 0
	8000,	// 1
	11025,	// 2
	16000,	// 3
	22050,	// 4
	32000	// 5
};

DMFConverter::DMFConverter(ESFOutput ** esfout) // ctor
{
    esf = *esfout;

    /* Initialize all the variables */
    SongType = 0;
    TickBase = 0;
    TickTimeEvenRow = 0;
    TickTimeOddRow = 0;
    RegionType = 0;
    CurrPattern = 0;
    CurrRow = 0;
    SkipPattern = 0;
    NextPattern = 0;
    NextRow = 0;
    TotalRowsPerPattern = 0;
    TotalPatterns = 0;
    PatternList = 0;
    ArpTickSpeed = 0;

    ChannelCount = 0;
    RegionType = 0;

	UseTables = false;
	VerboseLog = false;

    //NoiseMode = PSG_WHITE_NOISE_HI;
    for(int i=0; i<10; i++)
    {
        Channels[i].Id = CHANNEL_FM1;
        Channels[i].Type = CHANNEL_TYPE_FM;
        Channels[i].ESFId = ESF_FM1;
        //Channels[i].DACEnabled = 0;
        Channels[i].EffectCount = 0;
        Channels[i].Note = 0;
        Channels[i].Octave = 0;
        Channels[i].ToneFreq = 0;
        Channels[i].NoteFreq = 0;
        Channels[i].LastFreq = 0;
        Channels[i].NewFreq = 0;
        Channels[i].Instrument = 0xff;
        Channels[i].NewInstrument = 0;
        Channels[i].Volume = 0x7f;
		Channels[i].LastVolume = 0x7f;
        Channels[i].NewVolume = 0;
        Channels[i].SubtickFX = 0;
        LoopState[i] = Channels[i];
    }

    LoopFound = 0;
    LoopPattern = 0;
    LoopRow = 0;
    LoopFlag = 0;

    DACEnabled = 0;
    PSGNoiseFreq = 0;

    return;
}

DMFConverter::~DMFConverter() // dtor
{
    /* Free any allocated memory */
    delete [] PatternList;
    return;
}

/** @brief Initializes module **/
bool DMFConverter::Initialize(const char* Filename)
{
    /* Open file */
    streampos   file_size;
    ifstream file (Filename, ios::in|ios::binary|ios::ate);
    if (file.is_open())
    {
		if(VerboseLog)
		{
			fprintf(stderr, "Loading file: %s\n", Filename);
		}

        file_size = file.tellg();
        comp_data = new char [file_size];
        file.seekg (0, ios::beg);
        file.read (comp_data, file_size);
        file.close();

		uLong comp_size_max = 16777216;
		uLong comp_size = comp_size_max;
        data.resize(comp_size);

        /* Decompress the file with miniz */
        int res;
        #if DEBUG
            fprintf(stdout, "decompression buffer: %lu, original filesize: %d\n", comp_size, (int)file_size);
        #endif
        res = uncompress((Byte*) &data[0], &comp_size, (Byte*) comp_data, file_size);

		if(comp_size >= comp_size_max)
		{
			fprintf(stderr, "Buffer overflow (%u/%u bytes)", (uint32_t)comp_size, (uint32_t)comp_size_max);
		}
		else if(res != Z_OK)
        {
            fprintf(stderr, "Failed to uncompress: ");
            if(res == Z_MEM_ERROR)
                fprintf(stderr, "Not enough memory.\n");
            else if(res == Z_BUF_ERROR)
                fprintf(stderr, "Not enough room in the output buffer.\n");
            else
                fprintf(stderr, "Invalid or corrupted module?\n");
            return 1;
        }
        else
        {
            delete [] comp_data;

			//Create stream and serialise file
			Stream stream((char*)&data[0]);
			stream.Serialise(m_dmfFile);

            /* Decompression successful, now parse the DMF metadata */
            char DefleMagic[17];
            memcpy(DefleMagic, &data[0], 16);
            DefleMagic[16] = 0;
            res = strcmp(DefleMagic, ".DelekDefleMask.");
            if(res)
            {
                fprintf(stderr, "Not a valid DefleMask module.\n");
                return 1;
            }
            #if DEBUG
                fprintf(stdout, "Module version: %x\n", (int) m_dmfFile.m_fileVersion);
				fprintf(stdout, "System: %x\n", (int) m_dmfFile.m_systemType);
            #endif
            /* Get the DMF system. */
			System = (DMFSystem)m_dmfFile.m_systemType;
            if(System == DMF_SYSTEM_GENESIS)
            {
                /* This is a Genesis module */
                ChannelCount = 10;
                for(int i=0;i<ChannelCount;i++)
                {
                    Channels[i].Id = MDChannels[i].aChannelId;
                    Channels[i].Type = MDChannels[i].aChannelType;
                    Channels[i].ESFId = MDChannels[i].aESFChannel;
                }
            }
            else if(System == DMF_SYSTEM_SMS)
            {
                fprintf(stderr, "Master System module support is untested.\n");
                /* This is a Master System module */
                ChannelCount = 4;
                for(int i=0;i<ChannelCount;i++)
                {
                    Channels[i].Id = SMSChannels[i].aChannelId;
                    Channels[i].Type = SMSChannels[i].aChannelType;
                    Channels[i].ESFId = SMSChannels[i].aESFChannel;
                }
            }
            else
            {
                fprintf(stderr, "Only Sega Genesis modules are supported.\n"); // Bug: Should obviously be 'Mega Drive modules'
            }

            /* Extract useful module metadata */
			TickBase = m_dmfFile.m_timeBase;
			TickTimeEvenRow = m_dmfFile.m_tickTimeEven;
			TickTimeOddRow = m_dmfFile.m_tickTimeOdd;
			RegionType = m_dmfFile.m_framesMode;
			// custom HZ is ignored (4, 5, 6, 7)
			TotalRowsPerPattern = m_dmfFile.m_numNoteRowsPerPattern;
			TotalPatterns = m_dmfFile.m_numPatternPages;
			ArpTickSpeed = m_dmfFile.m_arpeggioTickSpeed;
			TotalInstruments = m_dmfFile.m_numInstruments;
			TotalSamples = m_dmfFile.m_numSamples;

			for(int i = 0; i < m_dmfFile.m_numInstruments; i++)
			{
				char filename[FILENAME_MAX] = { 0 };
				if(m_dmfFile.m_instruments[i].m_mode == DMFFile::INSTRUMENT_FM)
				{
					//sprintf_s(filename, FILENAME_MAX, "instr_FM_%02x_%s.eif", i + InstrumentOffset, m_dmfFile.m_instruments[i].m_name.c_str());
					snprintf(filename, FILENAME_MAX, "instr_%02x.eif", i + InstrumentOffset);
				}
				else
				{
					//sprintf_s(filename, FILENAME_MAX, "instr_PSG_%02x_%s.eif", i + InstrumentOffset, m_dmfFile.m_instruments[i].m_name.c_str());
					snprintf(filename, FILENAME_MAX, "instr_%02x.eif", i + InstrumentOffset);
				}

				OutputInstrument(i, filename);
			}

			for(int i = 0; i < m_dmfFile.m_numSamples; i++)
			{
				char filename[FILENAME_MAX] = { 0 };
				snprintf(filename, FILENAME_MAX, "instr_%02x.ewf", i + TotalInstruments + InstrumentOffset);
				OutputSample(i, filename, false);
			}

            /* Finally build the pattern offset table */
            PatternData = new uint32_t[ChannelCount];

            /* Get some pattern data */
            for(int i=0;i<ChannelCount;i++)
            {
				Channels[i].EffectCount = m_dmfFile.m_channels[i].m_numEffects;

                /* Check for backwards jumps */
                for(CurrPattern=0;CurrPattern<TotalPatterns;CurrPattern++)
                {
                    for(CurrRow=0;CurrRow<TotalRowsPerPattern;CurrRow++)
                    {
                        uint8_t EffectCounter;
                        for(EffectCounter=0;EffectCounter<Channels[i].EffectCount;EffectCounter++)
                        {
							uint8_t EffectType = m_dmfFile.m_channels[i].m_patternPages[CurrPattern].m_notes[CurrRow].m_effects[EffectCounter].m_effectType;
							uint8_t EffectParam = m_dmfFile.m_channels[i].m_patternPages[CurrPattern].m_notes[CurrRow].m_effects[EffectCounter].m_effectValue;
							if(EffectType == EFFECT_TYPE_JUMP) // jump
                            {
                                if(EffectParam <= CurrPattern && LoopFound == false)
                                {
                                    LoopFound = true;
                                    LoopPattern = EffectParam;
                                    LoopRow = 0;
									fprintf(stdout, "Loop from pattern %x to %x\n", (int)CurrPattern, (int)LoopPattern);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "File not found: %s\n", Filename);
        return 1;
    }
    return 0;
}

/** @brief Parses module and writes into ESF. **/
bool DMFConverter::Parse()
{
	if(VerboseLog)
	{
		fprintf(stdout, "Now parsing pattern data...\n");
	}

	esf->WaitCounter = 0;

	//Determine used channels
	for(uint8_t CurrChannel = 0; CurrChannel < ChannelCount; CurrChannel++)
	{
		bool Used = false;

		for(CurrPattern = 0; CurrPattern < TotalPatterns && !Used; CurrPattern++)
		{
			for(CurrRow = 0; CurrRow < TotalRowsPerPattern && !Used; CurrRow++)
			{
				uint16_t note = m_dmfFile.m_channels[CurrChannel].m_patternPages[CurrPattern].m_notes[CurrRow].m_note;
				if(note != 0 && note != NOTE_OFF)
				{
					UsedChannels.insert(CurrChannel);
					Used = true;
				}
			}
		}
	}

	if(PALMode)
	{
		//Set FM timer to play back PAL speed tracks
		esf->SetRegisterBank0(FMREG_26_TIMER_B, FM_TimerB_PAL);
	}

	if(LockChannels)
	{
		//Lock used channels
		for(std::set<uint8_t>::iterator it = UsedChannels.begin(), end = UsedChannels.end(); it != end; ++it)
		{
			esf->LockChannel(Channels[*it].ESFId);
		}
	}

	if(LoopWholeTrack)
	{
		esf->SetLoop();
	}

    NextRow = 0;
	for(CurrPattern=0;CurrPattern<TotalPatterns;CurrPattern++)
    {
        #if MODDATA
            fprintf(stdout, "Pattern %x\n",(int)CurrPattern);
        #endif
        CurrRow = NextRow;
        NextRow = 0;
        for(CurrRow=CurrRow;CurrRow<TotalRowsPerPattern;CurrRow++)
        {
            #if MODDATA
                fprintf(stdout, "Row %03d: ",(int)CurrRow);
            #endif
            esf->InsertPatRow(CurrPattern, CurrRow);

			/* Set loop if we're at the loop start */
			if(LoopFound == true && LoopPattern == CurrPattern && LoopRow == CurrRow)
				esf->SetLoop();

            /* Parse pattern data */
			for(uint8_t CurrChannel = 0; CurrChannel<ChannelCount; CurrChannel++)
            {
				if(this->ParseChannelRow(CurrChannel, CurrPattern, CurrRow))
                {
                    fprintf(stderr, "Could not parse module data.\n");
                    return 1;
                }
                //fprintf(stdout, "%#x, ", ChannelData);
            }

            #if MODDATA
                fprintf(stdout, "\n");
            #endif

            //Calculate number of ticks per row - Deflemask exports 1 tick time for even rows, and another for odd rows
			uint8_t ticksPerRow = (CurrRow & 1) ? (TickTimeOddRow*(TickBase + 1)) : ( TickTimeEvenRow*(TickBase + 1));

			//Increment delay counter, will be used and cleared on next command
            esf->WaitCounter += ticksPerRow;

			//Process at least one active effect tick, and continue whilst idle (waitCounter > 0)
			int numEffectsProcessed = 0;
			do
			{
				numEffectsProcessed = 0;

				//Check if any effects need processing
				bool processEffects = false;
				bool wholeDelay = false;
				bool noteOn = false;
				for(uint8_t CurrChannel = 0; CurrChannel < ChannelCount; CurrChannel++)
				{
					EffectStage stage = GetActiveEffectStage(CurrChannel);
					if(stage != EFFECT_STAGE_OFF)
					{
						processEffects = true;

						if(stage == EFFECT_STAGE_INITIALISE)
						{
							//First use of effect, delay treated as note on
							wholeDelay = true;
						}

						if(Channels[CurrChannel].m_effectPortmento.NoteOnthisTick)
						{
							noteOn = true;
						}
					}
				}

				if(processEffects)
				{
					//If note on this row, don't process any delay
					if(!noteOn)
					{
						if(wholeDelay)
						{
							//First tick for at least one of the effects, process whole delay
							esf->Wait();
						}
						else
						{
							//Subsequent tick, delay by 1
							uint32_t oldDelay = esf->WaitCounter;
							esf->WaitCounter = 1;
							esf->Wait();
							esf->WaitCounter = oldDelay;
						}
					}

					for(uint8_t CurrChannel = 0; CurrChannel < ChannelCount; CurrChannel++)
					{
						numEffectsProcessed += ProcessActiveEffects(CurrChannel);
					}

					if(numEffectsProcessed && esf->WaitCounter > 0)
					{
						esf->WaitCounter -= 1;
					}
				}
			} while(esf->WaitCounter > 0 && numEffectsProcessed > 0);

            /* Are we at the loop end? If so, start playing the loop row */
            if(LoopFlag == true)
            {
                #if MODDATA
                    fprintf(stdout, "Loop:    ");
                #endif
					for(uint8_t CurrChannel=0;CurrChannel<ChannelCount;CurrChannel++)
                {
                    if(this->ParseChannelRow(CurrChannel, CurrPattern, CurrRow))
                        return 1;
                }
                #if MODDATA
                    fprintf(stdout, "\n");
                #endif
                esf->GotoLoop();
                break;
            }

            /* Do pattern jumps */
            if(SkipPattern)
            {
                SkipPattern = 0;
                CurrPattern = NextPattern-1;
                break;
            }
        }
        if(LoopFlag == true)
            break;
    }

	if(LoopWholeTrack)
	{
		esf->GotoLoop();
	}

    esf->StopPlayback();

    return 0;
}
/** @brief Parses pattern data for a single channel **/
bool DMFConverter::ParseChannelRow(uint8_t chan, uint32_t CurrPattern, uint32_t CurrRow)
{
	Channel& channel = Channels[chan];

	//Calculate number of ticks per row - Deflemask exports 1 tick time for even rows, and another for odd rows
	uint8_t ticksPerRow = (CurrRow & 1) ? (TickTimeOddRow * (TickBase + 1)) : (TickTimeEvenRow * (TickBase + 1));

    uint8_t EffectCounter;
    uint8_t EffectType;
    uint8_t EffectParam;

    /* Clean up effects */
    Channels[chan].m_effectNoteCut.NoteCut = EFFECT_OFF;
    Channels[chan].m_effectNoteDelay.NoteDelay = EFFECT_OFF;

    /* Get row data */
	Channels[chan].Note = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_note;
	Channels[chan].Octave = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_octave;
	Channels[chan].NewVolume = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_volume;
	Channels[chan].NewInstrument = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_instrument;

	uint8_t nextNote = 0;
	uint8_t nextOctave = 0;

	if(CurrRow < m_dmfFile.m_numNoteRowsPerPattern - 1)
	{
		nextNote = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow + 1].m_note;
		nextOctave = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow + 1].m_octave;
	}

    /* Instrument updated? */
    if(Channels[chan].NewInstrument != Channels[chan].Instrument && Channels[chan].NewInstrument != 0xff)
    {
        Channels[chan].Instrument = Channels[chan].NewInstrument;
        if(UseTables)
            esf->SetInstrument(Channels[chan].ESFId,InstrumentTable[Channels[chan].Instrument]);
        else
            esf->SetInstrument(Channels[chan].ESFId,Channels[chan].Instrument);

        /* Echo resets the volume if the instrument is changed (FM only...) */
		if(Channels[chan].Type != CHANNEL_TYPE_PSG && Channels[chan].Type != CHANNEL_TYPE_PSG4)
		{
			Channels[chan].Volume = 0x7f;
			Channels[chan].LastVolume = 0x7f;
		}
    }

    /* Volume updated? */
    if(Channels[chan].NewVolume != Channels[chan].Volume && Channels[chan].NewVolume != 0xff)
    {
        Channels[chan].Volume = Channels[chan].NewVolume;
		Channels[chan].LastVolume = Channels[chan].NewVolume;
        if(Channels[chan].Type == CHANNEL_TYPE_FM || CHANNEL_TYPE_FM6)
            esf->SetVolume(Channels[chan].ESFId,(Channels[chan].Volume));
        else if(Channels[chan].Type == CHANNEL_TYPE_PSG || CHANNEL_TYPE_PSG4)
            esf->SetVolume(Channels[chan].ESFId,(Channels[chan].Volume));
    }

    /* Parse some effects before any note ons */
    for(EffectCounter=0;EffectCounter<Channels[chan].EffectCount;EffectCounter++)
    {
		EffectType = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_effects[EffectCounter].m_effectType;
		EffectParam = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_effects[EffectCounter].m_effectValue;
		if(EffectType == EFFECT_TYPE_DAC_ON) // DAC enable
        {
            DACEnabled = 0;

            if(EffectParam > 0)
                DACEnabled = 1;

            //fprintf(stderr, "effect %02x%02x: DAC enable %d\n",(int)EffectType,(int)EffectParam,(int)DACEnabled);
        }
		else if(EffectType == EFFECT_TYPE_PSG_NOISE) // Set noise mode
        {
            PSGNoiseFreq = 0;
            PSGPeriodicNoise = 0;

            if(EffectParam & 0xF0)
                PSGNoiseFreq = 1;
            if(EffectParam & 0x0F)
                PSGPeriodicNoise = 1;

			if(VerboseLog)
			{
				fprintf(stdout, "psg val = %d %d\n", (int)PSGNoiseFreq, (int)PSGPeriodicNoise);
			}
            //fprintf(stderr, "effect %02x%02x: PSG noise mode %d %d\n",(int)EffectType,(int)EffectParam,(int)PSGNoiseFreq,(int)PSGPeriodicNoise);
        }
    }

    /* Is this a note off? */
	if(Channels[chan].Note == NOTE_OFF)
    {
        #if MODDATA
            fprintf(stdout, "OFF ");
        #endif
        esf->NoteOff(Channels[chan].ESFId);

        Channels[chan].ToneFreq = 0;
        Channels[chan].LastFreq = 0;
        Channels[chan].NewFreq = 0;

		//Turn off effects which stop at note off
		channel.m_effectPortaNote.PortaNote = EFFECT_OFF;
		channel.m_effectPortmento.Porta == EFFECT_OFF;
		channel.m_effectVolSlide.VolSlide = EFFECT_OFF;
    }
    /* Note on? */
    else if(Channels[chan].Note != 0)
    {
		//Save last note/octave for effects in subsequent rows
		Channels[chan].LastNote = Channels[chan].Note;
		Channels[chan].LastOctave = Channels[chan].Octave;

        /* Convert octave/note format to something that makes more sense */
        if(Channels[chan].Note == 12)
        {
            Channels[chan].Octave++;
            Channels[chan].Note = 0;
        }

        #if MODDATA
            fprintf(stdout, "%s%x ",NoteNames[Channels[chan].Note].c_str(),(int)Channels[chan].Octave);
        #endif

		//Turn off effects which stop at next note
		channel.m_effectPortaNote.PortaNote = EFFECT_OFF;
		channel.m_effectPortmento.Porta = EFFECT_OFF;
		channel.m_effectVolSlide.VolSlide = EFFECT_OFF;

        /* Parse some effects that will affect the note on */
        for(EffectCounter=0;EffectCounter<Channels[chan].EffectCount;EffectCounter++)
        {
			EffectType = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_effects[EffectCounter].m_effectType;
			EffectParam = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_effects[EffectCounter].m_effectValue;
            if(EffectType == 0x03) // Tone portamento.
				Channels[chan].m_effectPortaNote.PortaNote = EFFECT_SCHEDULE;
            else if(EffectType == 0xed) // Note delay.
            {
                Channels[chan].m_effectNoteDelay.NoteDelay = EFFECT_SCHEDULE;
				Channels[chan].m_effectNoteDelay.NoteDelayOffset = EffectParam;
            }
        }

        /* If note delay or tone portamento is off, send the note on command already! */
		if(Channels[chan].m_effectNoteDelay.NoteDelay == EFFECT_OFF || Channels[chan].m_effectPortaNote.PortaNote == EFFECT_OFF)
            this->NoteOn(chan);
    }
    /* Note column is empty */
    else {
        #if MODDATA
            fprintf(stdout, "--- ");
        #endif
    }

    #if MODDATA
    fprintf(stdout, "%02x %02x, ",(int)Channels[chan].NewVolume,(int)Channels[chan].NewInstrument);
    #endif

	if(0) //Channels[chan].m_effectPortaNote.PortaNote != EFFECT_OFF)
	{
		//TODO: handle octave change
		uint8_t currentNote = Channels[chan].m_effectPortaNote.PortaNoteCurrentNote;
		uint8_t currentOctave = Channels[chan].m_effectPortaNote.PortaNoteCurrentOctave;
		uint8_t targetNote = Channels[chan].m_effectPortaNote.PortaNoteTargetNote;
		uint8_t targetOctave = Channels[chan].m_effectPortaNote.PortaNoteTargetOctave;
		uint8_t speed = Channels[chan].m_effectPortaNote.PortaNoteSpeed;

		//Lerp towards target at speed
		if((targetNote | targetOctave << 8) > (currentNote | currentOctave << 8))
		{
			currentNote += speed;
			if(currentNote > targetNote)
				currentNote = targetNote;
		}
		else if((targetNote | targetOctave << 8) < (currentNote | currentOctave << 8))
		{
			currentNote -= speed;
			if(currentNote < targetNote)
				currentNote = targetNote;
		}

		//If target reached, effect finished
		if(currentNote == targetNote)
		{
			Channels[chan].m_effectPortaNote.PortaNote == EFFECT_OFF;
		}

		//Note on
		Channels[chan].Note = currentNote;
		NoteOn(chan);

		//Update effect history data
		Channels[chan].m_effectPortaNote.PortaNoteCurrentNote = currentNote;
		Channels[chan].m_effectPortaNote.PortaNoteCurrentOctave = currentOctave;
	}

	if(channel.m_effectVolSlide.VolSlide != EFFECT_OFF)
	{
		int i = 0;

		for(i = 0; i < ticksPerRow && channel.m_effectVolSlide.VolSlide != EFFECT_OFF; i++)
		{
			//Calc current volume
			channel.m_effectVolSlide.CurrVol = Clamp(channel.m_effectVolSlide.CurrVol + channel.m_effectVolSlide.VolSlideValue, 0, 0x7f);

			//Set volume (includes current delay)
			esf->SetVolume(channel.ESFId, channel.m_effectVolSlide.CurrVol);

			//Delay 1 tick
			esf->WaitCounter += 1;

			//Set channel volume history
			channel.LastVolume = channel.m_effectVolSlide.CurrVol;

			//If hit the volume limits, finished
			if(channel.m_effectVolSlide.CurrVol == 0 || channel.m_effectVolSlide.CurrVol == 0x7f)
			{
				channel.m_effectVolSlide.VolSlide = EFFECT_OFF;
			}
		}

		//Decrease existing tick count
		esf->WaitCounter -= i;
	}

    //Process new effects
    for(EffectCounter=0;EffectCounter<Channels[chan].EffectCount;EffectCounter++)
    {
		EffectType = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_effects[EffectCounter].m_effectType;
		EffectParam = m_dmfFile.m_channels[chan].m_patternPages[CurrPattern].m_notes[CurrRow].m_effects[EffectCounter].m_effectValue;
        //fprintf(stdout, "%02x %02x, ",(int)EffectType,(int)EffectParam);
        switch(EffectType)
        {
            /* Normal effects */
		case EFFECT_TYPE_ARPEGGIO: // Arpeggio
            if(EffectParam != 0)
            {
                Channels[chan].m_effectArpeggio.Arp = EFFECT_NORMAL;
                uint8_t ArpOct;
                uint8_t ArpNote;

                /* do first freq */
                ArpOct = Channels[chan].Octave;
                ArpNote = Channels[chan].Note + (EffectParam & 0xf0 >> 4);
                while(ArpNote > 12)
                {
                    ArpOct++;
                    ArpNote-=12;
                }

                /* PSG */
                if(Channels[chan].Type == CHANNEL_TYPE_PSG)
					Channels[chan].m_effectArpeggio.Arp1 = PSGFreqs[ArpNote][ArpOct - 2];
                else
					Channels[chan].m_effectArpeggio.Arp1 = (Channels[chan].Octave << 11) | FMFreqs[Channels[chan].Note];

				Channels[chan].m_effectArpeggio.Arp2 = 0;

                /* do second freq */
                if((EffectParam & 0x0f) > 0)
                {
                    ArpOct = Channels[chan].Octave;
                    ArpNote = Channels[chan].Note + (EffectParam & 0xf0 >> 4);
                    while(ArpNote > 12)
                    {
                        ArpOct++;
                        ArpNote-=12;
                    }

                    /* PSG */
                    if(Channels[chan].Type == CHANNEL_TYPE_PSG)
						Channels[chan].m_effectArpeggio.Arp2 = PSGFreqs[ArpNote][ArpOct - 2];
                    else
						Channels[chan].m_effectArpeggio.Arp2 = (Channels[chan].Octave << 11) | FMFreqs[Channels[chan].Note];
                }

            }
            else
            {
				Channels[chan].m_effectArpeggio.Arp = EFFECT_OFF;
            }
            break;
		case EFFECT_TYPE_PORTMENTO_UP: // Portamento up
		case EFFECT_TYPE_PORTMENTO_DOWN: // Portamento down
			//TODO: Fix for PSG
			if(chan < CHANNEL_PSG1)
			{
				if(EffectParam == 0)
				{
					channel.m_effectPortmento.Porta = EFFECT_OFF;
					channel.m_effectPortmento.Stage = EFFECT_STAGE_OFF;
				}
				else
				{
					channel.m_effectPortmento.NoteOnthisTick = (channel.Note != 0 || channel.Octave != 0);

					//If note on this tick or effect was off, start from last note/octave
					if(channel.m_effectPortmento.NoteOnthisTick || channel.m_effectPortmento.Porta == EFFECT_OFF)
					{
						channel.m_effectPortmento.Semitone = FMFreqs[channel.LastNote];
						channel.m_effectPortmento.Octave = channel.LastOctave;
						channel.m_effectPortmento.Stage = EFFECT_STAGE_INITIALISE;
					}

					channel.m_effectPortmento.Porta = EffectType == 0x01 ? EFFECT_UP : EFFECT_DOWN;
					channel.m_effectPortmento.PortaSpeed = EffectParam;
				}
			}
            break;
		case EFFECT_TYPE_PORTMENTO_TO_NOTE: // Tone portamento
			//channel.m_effectPortaNote.PortaNote = EFFECT_UP;
			//channel.m_effectPortaNote.PortaNoteSpeed = EffectParam;
			//channel.m_effectPortaNote.PortaNoteCurrentNote = Channels[chan].Note;
			//channel.m_effectPortaNote.PortaNoteCurrentOctave = Channels[chan].Octave;
			//channel.m_effectPortaNote.PortaNoteTargetNote = nextNote;
			//channel.m_effectPortaNote.PortaNoteTargetOctave = nextOctave;
            break;
		case EFFECT_TYPE_PAN: // Set panning
            if(Channels[chan].Type == CHANNEL_TYPE_FM || Channels[chan].Type == CHANNEL_TYPE_FM6)
                esf->SetParams(Channels[chan].ESFId,(EffectParam & 0x10)<<3 | (EffectParam & 0x01)<<6);
            break;
		case EFFECT_TYPE_SET_SPEED_1: // Set speed 1
            TickTimeEvenRow = EffectParam;
            break;
		case EFFECT_TYPE_SET_SPEED_2: // Set speed 2
            TickTimeOddRow = EffectParam;
            break;
		case EFFECT_TYPE_VOLUME_SLIDE: // Volume slide
		{
			int upSlide = (EffectParam & 0xF0) >> 4;
			int downSlide = -(EffectParam & 0x0F);
			channel.m_effectVolSlide.VolSlideValue = upSlide + downSlide;
			channel.m_effectVolSlide.VolSlide = (channel.m_effectVolSlide.VolSlideValue > 0) ? EFFECT_UP : EFFECT_DOWN;
			channel.m_effectVolSlide.CurrVol = channel.LastVolume;
			break;
		}
		case EFFECT_TYPE_JUMP: // Position jump
            if(LoopFound == true && EffectParam <= CurrPattern && LoopFlag == false)
                LoopFlag = true;
            break;
		case EFFECT_TYPE_BREAK: // Pattern break
            SkipPattern = true;
            if(CurrPattern != TotalPatterns)
                NextPattern = CurrPattern+1;
            else // should actually be the same as loop
                NextPattern = 0;
            NextRow = EffectParam;
            break;
            /* Unknown/unsupported effects */
        default:
            break;
        }
    }

    return 0;
}

EffectStage DMFConverter::GetActiveEffectStage(uint8_t chan)
{
	//TODO: Other effects
	return Channels[chan].m_effectPortmento.Porta == EFFECT_OFF ? EFFECT_STAGE_OFF : Channels[chan].m_effectPortmento.Stage;
}

int DMFConverter::ProcessActiveEffects(uint8_t chan)
{
	Channel& channel = Channels[chan];

	int numEffectsProcess = 0;

	//Process active effects
	if(channel.m_effectPortmento.Porta != EFFECT_OFF)
	{
		if(channel.m_effectPortmento.NoteOnthisTick)
		{
			//Had note on this row, skip
			channel.m_effectPortmento.NoteOnthisTick = false;
		}
		else
		{
			//Increment/decrement frequency
			if(channel.m_effectPortmento.Porta == EFFECT_UP)
			{
				channel.m_effectPortmento.Semitone += channel.m_effectPortmento.PortaSpeed;

				//Clamp to max octave+freq
				if(channel.m_effectPortmento.Octave == MaxOctave && channel.m_effectPortmento.Semitone > FMFreqs[MaxFMFreqs - 1])
				{
					channel.m_effectPortmento.Semitone = FMFreqs[MaxFMFreqs - 1];
					channel.m_effectPortmento.Porta = EFFECT_OFF;
				}
			}
			else if(channel.m_effectPortmento.Porta == EFFECT_DOWN)
			{
				channel.m_effectPortmento.Semitone -= channel.m_effectPortmento.PortaSpeed;

				//Clamp to min octave+freq
				if(channel.m_effectPortmento.Octave == 0 && channel.m_effectPortmento.Semitone < FMFreqs[0])
				{
					channel.m_effectPortmento.Semitone = FMFreqs[0];
					channel.m_effectPortmento.Porta = EFFECT_OFF;
				}
			}

			//Wrap around octave
			if(channel.m_effectPortmento.Semitone < FMFreqs[0])
			{
				channel.m_effectPortmento.Semitone = FMFreqs[MaxFMFreqs - 1];
				channel.m_effectPortmento.Octave--;
			}

			if(channel.m_effectPortmento.Semitone > FMFreqs[MaxFMFreqs - 1])
			{
				channel.m_effectPortmento.Semitone = FMFreqs[0];
				channel.m_effectPortmento.Octave++;
			}

			//Set frequency
			channel.Octave = channel.m_effectPortmento.Octave;
			SetFrequency(chan, channel.m_effectPortmento.Semitone, false);
		}

		//Continue until next note off
		numEffectsProcess++;
		channel.m_effectPortmento.Stage = EFFECT_STAGE_CONTINUE;
	}

	return numEffectsProcess;
}

void DMFConverter::NoteOn(uint8_t chan)
{
    /* Is this the PSG noise channel? */
    if (Channels[chan].Type == CHANNEL_TYPE_PSG4)
    {
        uint8_t NoiseMode = 0;

        /* Is periodic noise active? */
        if(PSGPeriodicNoise)
            NoiseMode = 4;

        /* Are we using the PSG3 frequency? */
        if(PSGNoiseFreq)
        {
            // if C-8 change it to B-7
            if(Channels[chan].Note == 0 && Channels[chan].Octave == 8)
            {
                Channels[chan].Octave--;
                Channels[chan].Note = 11;
            }
            Channels[chan].Octave--;

            /* Get the frequency value */
            Channels[chan-1].ToneFreq = PSGFreqs[Channels[chan].Note][Channels[chan].Octave];

            /* Only update frequency if it's not the same as the last */
            if(Channels[chan-1].LastFreq != Channels[chan-1].ToneFreq)
                esf->SetFrequency(Channels[chan-1].ESFId,Channels[chan-1].ToneFreq);

            Channels[chan-1].LastFreq = Channels[chan-1].ToneFreq;

            NoiseMode = NoiseMode + 3;

        }
        else {
            switch(Channels[chan].Note)
            {
            default:
            case 3: // D
                NoiseMode++;
            case 2: // C#
                NoiseMode++;
            case 1: // C
                break;
            }

			if(VerboseLog)
			{
				fprintf(stdout, "noisemode = %d\n", (int)NoiseMode);
			}
        }

        esf->NoteOn(Channels[chan].ESFId,NoiseMode);
    }

    /* Skip if this is the PSG3 channel and its frequency value is already used for the noise channel */
	else if(!(Channels[chan].Id == CHANNEL_PSG3 && PSGNoiseFreq))
    {
        /* Calculate frequency */
        Channels[chan].ToneFreq = 0;    // Reset if this is for a channel where this doesn't make much sense.

        /* Is this an FM channel? */
        if(Channels[chan].Type == CHANNEL_TYPE_FM || (Channels[chan].Type == CHANNEL_TYPE_FM6 && DACEnabled == false))
        {
            Channels[chan].ToneFreq = (Channels[chan].Octave<<11)|FMFreqs[Channels[chan].Note];
        }
        /* PSG */
        else if(Channels[chan].Type == CHANNEL_TYPE_PSG)
        {
            Channels[chan].Octave--;
            Channels[chan].ToneFreq = PSGFreqs[Channels[chan].Note][Channels[chan].Octave];
        }

        /* Reset last tone / new tone freqs */
        Channels[chan].NoteFreq = Channels[chan].ToneFreq;
        Channels[chan].LastFreq = 0;
        Channels[chan].NewFreq = 0;

        if((Channels[chan].Type == CHANNEL_TYPE_FM6 && DACEnabled == true))
        {
			int sampleIdx = 0;

			if(UseTables)
				sampleIdx = SampleTable[Channels[chan].Note];
            else
				sampleIdx = Channels[chan].Note;

			//Samples at end of instrument table
			int pcmInstrIdx = TotalInstruments + InstrumentOffset + sampleIdx;

			//PCM note on
			esf->NoteOn(ESF_DAC, pcmInstrIdx);
        }
        else
        {
            esf->NoteOn(Channels[chan].ESFId,Channels[chan].Note,Channels[chan].Octave);
        }
    }
    return;
}

void DMFConverter::SetFrequency(uint8_t chan, uint32_t FMSemitone, bool processDelay)
{
    /* Is this the PSG noise channel? */
    if (Channels[chan].Type == CHANNEL_TYPE_PSG4)
    {
        uint8_t NoiseMode = 0;

        /* Is periodic noise active? */
        if(PSGPeriodicNoise)
            NoiseMode = 4;

        /* Are we using the PSG3 frequency? */
        if(PSGNoiseFreq)
        {
            // if C-8 change it to B-7
            if(Channels[chan].Note == 0 && Channels[chan].Octave == 8)
            {
                Channels[chan].Octave--;
                Channels[chan].Note = 11;
            }
            Channels[chan].Octave--;

            /* Get the frequency value */
            Channels[chan-1].ToneFreq = PSGFreqs[Channels[chan].Note][Channels[chan].Octave];

            /* Only update frequency if it's not the same as the last */
            if(Channels[chan-1].LastFreq != Channels[chan-1].ToneFreq)
                esf->SetFrequency(Channels[chan-1].ESFId,Channels[chan-1].ToneFreq, processDelay);

            Channels[chan-1].LastFreq = Channels[chan-1].ToneFreq;

            NoiseMode = NoiseMode + 3;

        }
    }

    /* Skip if this is the PSG3 channel and its frequency value is already used for the noise channel */
	else if(!(Channels[chan].Id == CHANNEL_PSG3 && PSGNoiseFreq))
    {
        /* Calculate frequency */
        Channels[chan].ToneFreq = 0;    // Reset if this is for a channel where this doesn't make much sense.

        /* Is this an FM channel? */
        if(Channels[chan].Type == CHANNEL_TYPE_FM || (Channels[chan].Type == CHANNEL_TYPE_FM6 && DACEnabled == false))
        {
			Channels[chan].ToneFreq = (Channels[chan].Octave << 11) | FMSemitone; // FMFreqs[Channels[chan].Note];
        }
        /* PSG */
        else if(Channels[chan].Type == CHANNEL_TYPE_PSG)
        {
            Channels[chan].Octave--;
            Channels[chan].ToneFreq = PSGFreqs[Channels[chan].Note][Channels[chan].Octave];
        }

		if(!(Channels[chan].Type == CHANNEL_TYPE_FM6 && DACEnabled == true))
		{
			if(Channels[chan].LastFreq != Channels[chan].ToneFreq)
			{
				esf->SetFrequency(Channels[chan].ESFId, Channels[chan].ToneFreq, processDelay);
			}
		}

        /* Reset last tone / new tone freqs */
        Channels[chan].NoteFreq = Channels[chan].ToneFreq;
        Channels[chan].LastFreq = 0;
        Channels[chan].NewFreq = 0;
    }
    return;
}

/** Extracts FM instrument data and stores as params for the EIF macro */
void DMFConverter::OutputInstrument(int instrumentIdx, const char* filename)
{
    static int optable[] = {1,2,3,4};
    static int8_t dttable[] = {-3,-2,-1,0,1,2,3};

	if(m_dmfFile.m_instruments[instrumentIdx].m_mode == DMFFile::INSTRUMENT_FM)
	{
		DMFFile::Instrument::ParamDataFM& paramDataIn = m_dmfFile.m_instruments[instrumentIdx].m_paramsFM;
		ESFFile::ParamDataFM paramDataOut;

		paramDataOut.alg_fb = (paramDataIn.alg | (paramDataIn.fb << 3));

		if(VerboseLog)
		{
			fprintf(stdout, "alg  = %d\n", (int)paramDataIn.alg);  //ALG
			fprintf(stdout, "fb  = %d\n", (int)paramDataIn.fb);  //FB
		}

		for(int i = 0; i < DMFFile::sMaxOperators; i++)
		{
			DMFFile::Instrument::ParamDataFM::Operator& opData = paramDataIn.m_operators[i];

			int op = optable[i];

			if(VerboseLog)
			{
				fprintf(stdout, "ar%d  = %d\n", op, (int)opData.ar);  //AR
				fprintf(stdout, "dr%d  = %d\n", op, (int)opData.dr);  //DR
				fprintf(stdout, "d2r%d  = %d\n", op, (int)opData.d2r); //D2R
				fprintf(stdout, "rr%d  = %d\n", op, (int)opData.rr);  //RR
				fprintf(stdout, "tl%d  = %d\n", op, (int)opData.tl); //TL
				fprintf(stdout, "sl%d  = %d\n", op, (int)opData.sl); //SL
				fprintf(stdout, "mul%d = %d\n", op, (int)opData.mul); //MULT
				fprintf(stdout, "dt%d  = %d\n", op, dttable[opData.dt]); //DT
				fprintf(stdout, "rs%d  = %d\n", op, (int)opData.rs); //RS
				fprintf(stdout, "ssg%d = $%02x\n", op, (int)opData.ssg);//SSG-EG
			}

			// Detune = -3 to 3, bit 4 is primitive, inverted sign
			uint8_t dt = 0;
			if(dttable[opData.dt] < 0)
			{
				dt = (int)abs(dttable[opData.dt]) & 0x3;
				dt |= 0x4;
			}
			else
			{
				dt = dttable[opData.dt] & 0x3;
			}

			paramDataOut.mul[i] = (opData.mul | (dt << 4));
			paramDataOut.tl[i] = opData.tl;
			paramDataOut.ar_rs[i] = (opData.ar | (opData.rs << 6));
			paramDataOut.dr[i] = (opData.dr | (opData.am << 7));
			paramDataOut.sr[i] = opData.d2r;
			paramDataOut.rr_sl[i] = (opData.rr | (opData.sl << 4));
			paramDataOut.ssg[i] = opData.ssg;
		}

		if(FILE* file = fopen(filename, "wb"))
		{
			fwrite((char*)&paramDataOut, sizeof(ESFFile::ParamDataFM), 1, file);
			fclose(file);
		}

		if(VerboseLog)
		{
			fprintf(stdout, "\teif\n; end of FM instrument\n");
		}
	}
	else
	{
		DMFFile::Instrument::ParamDataPSG& paramDataIn = m_dmfFile.m_instruments[instrumentIdx].m_paramsPSG;

		//Create envelope data (no loop = end stream with mute loop (FE 0F EE))
		const int loopDataSize = (paramDataIn.envelopeVolume.loopPosition == 255) ? 3 : 1;
		const int dataSize = paramDataIn.envelopeVolume.envelopeSize + loopDataSize + 1;
		const int streamEnd = dataSize - loopDataSize - 1;

		uint8_t* data = new uint8_t[dataSize];

		int offset = 0;
		int volumeIdx = 0;
		while(offset < dataSize)
		{
			if(offset == streamEnd)
			{
				//End of data
				if(loopDataSize == 1)
				{
					data[offset++] = 0xFF;
				}
				else
				{
					data[offset++] = 0xFE;
					data[offset++] = 0x0F;
					data[offset++] = 0xFF;
				}
			}
			else if(offset == paramDataIn.envelopeVolume.loopPosition)
			{
				//Loop start
				data[offset++] = 0xFE;
			}
			else
			{
				data[offset++] = 0xF - paramDataIn.envelopeVolume.envelopeData[volumeIdx];
				volumeIdx++;
			}
		}

		if(FILE* file = fopen(filename, "w"))
		{
			fwrite((char*)data, dataSize, 1, file);
			fclose(file);
		}

		if(VerboseLog)
		{
			fprintf(stdout, "\teif\n; end of PSG instrument\n");
		}
	}

    return;
}

void DMFConverter::OutputSample(int sampleIdx, const char* filename, bool convertFormat)
{
	const DMFFile::Sample& sample = m_dmfFile.m_samples[sampleIdx];

	//If correct format, skip conversion
	const int toleranceHz = 100;

	if(!convertFormat || (sample.m_bitsPerSample == 8 && abs(DMFFile::sSampleRates[sample.m_sampleRate] - DMFFile::sTargetSampleRate) <= toleranceHz))
	{
		uint32_t outputSize = sample.m_sampleSize + 1;

		uint8_t* destDataUint8 = new uint8_t[outputSize];

		for(int i = 0; i < outputSize - 1; i++)
		{
			destDataUint8[i] = ((uint8_t)sample.m_sampleData[i] & 0xFF);

			//Nudge 0xFF bytes to 0xFE
			if(destDataUint8[i] == 0xFF)
			{
				destDataUint8[i] = 0xFE;
			}
		}

		//End of data
		destDataUint8[outputSize - 1] = 0xFF;

		if(FILE* file = fopen(filename, "w"))
		{
			fwrite((char*)destDataUint8, outputSize, 1, file);
			fclose(file);
		}

		if(VerboseLog)
		{
			fprintf(stdout, "\tewf\n; end of sample\n");
		}

		delete destDataUint8;
	}
	else
	{
		float* sourceDataFloat = new float[sample.m_sampleSize];
		float* destDataFloat = new float[sample.m_sampleSize * 2];

		if(sample.m_bitsPerSample == 8)
		{
			//Source data to float
			for(int i = 0; i < sample.m_sampleSize; i++)
			{
				sourceDataFloat[i] = (float)((uint8_t)sample.m_sampleData[i] & 0xFF) / 255.0f;
			}
		}
		else if(sample.m_bitsPerSample == 16)
		{
			//Source data to float
			for(int i = 0; i < sample.m_sampleSize; i++)
			{
				sourceDataFloat[i] = (float)sample.m_sampleData[i] / 32767.0f;
			}
		}

		//Sample rate conversion using Secret Rabbit Code (http://www.mega-nerd.com/SRC)
		SRC_DATA srcConfig;
		srcConfig.data_in = sourceDataFloat;
		srcConfig.data_out = destDataFloat;
		srcConfig.input_frames = sample.m_sampleSize;
		srcConfig.output_frames = sample.m_sampleSize * 2;
		srcConfig.src_ratio = (double)DMFFile::sTargetSampleRate / (double)DMFFile::sSampleRates[sample.m_sampleRate];

		int srcResult = src_simple(&srcConfig, SRC_SINC_BEST_QUALITY, 1);
		if(srcResult == 0)
		{
			//Convert back to u8
			uint32_t outputSize = srcConfig.output_frames_gen + 1;

			uint8_t* destDataUint8 = new uint8_t[outputSize];

			for(int i = 0; i < outputSize - 1; i++)
			{
				destDataUint8[i] = (uint8_t)((destDataFloat[i] + 1.0f) * 128.0f);

				//Nudge 0xFF bytes to 0xFE
				if(destDataUint8[i] == 0xFF)
				{
					destDataUint8[i] = 0xFE;
				}
			}

			//End of data
			destDataUint8[outputSize - 1] = 0xFF;

			if(FILE* file = fopen(filename, "w"))
			{
				fwrite((char*)destDataUint8, outputSize, 1, file);
				fclose(file);
			}

			if(VerboseLog)
			{
				fprintf(stdout, "\tewf\n; end of sample\n");
			}

			delete destDataUint8;
		}
		else
		{
			//SRC error
			fprintf(stdout, "\tewf\n; sample rate conversion error\n");
		}

		delete sourceDataFloat;
		delete destDataFloat;
	}
}

void DMFFile::Serialise(Stream& stream)
{
	//Format string, version, system, song and author name
	stream.Serialise(m_formatString, sFormatStringSize);
	stream.Serialise(m_fileVersion);
	stream.Serialise(m_systemType);
	stream.Serialise(m_songName);
	stream.Serialise(m_songAuthor);

	//Column highlighting
	stream.Serialise(m_highlightA);
	stream.Serialise(m_highlightB);

	//Timing
	stream.Serialise(m_timeBase);
	stream.Serialise(m_tickTimeEven);
	stream.Serialise(m_tickTimeOdd);
	stream.Serialise(m_framesMode);
	stream.Serialise(m_usingCustomHz);
	stream.Serialise(m_customHz1);
	stream.Serialise(m_customHz2);
	stream.Serialise(m_customHz3);

	if(m_fileVersion >= DMFVersion_12_0)
	{
		stream.Serialise(m_numNoteRowsPerPattern);
	}
	else
	{
		uint8_t numNoteRowsByte = (uint8_t)m_numNoteRowsPerPattern;
		stream.Serialise(numNoteRowsByte);
		m_numNoteRowsPerPattern = numNoteRowsByte;
	}

	stream.Serialise(m_numPatternPages);

	if(m_fileVersion < DMFVersion_12_0)
	{
		stream.Serialise(m_arpeggioTickSpeed);
	}

	//Channels
	const int channelCount = ChannelCount[m_systemType];
	for(int i = 0; i < channelCount; i++)
	{
		m_patternMatrix[i] = new uint8_t[m_numPatternPages];
		for(int j = 0; j < m_numPatternPages; j++)
		{
			stream.Serialise(m_patternMatrix[i][j]);
		}
	}

	//Instruments
	stream.Serialise(m_numInstruments);
	for(int i = 0; i < m_numInstruments; i++)
	{
		stream.Serialise(m_instruments[i]);
	}

	//Wave tables
	stream.Serialise(m_numWaveTables);
	for(int i = 0; i < m_numWaveTables; i++)
	{
		stream.Serialise(m_waveTables[i]);
	}

	//Note channels
	for(int channelIdx = 0; channelIdx < channelCount; channelIdx++)
	{
		Channel& channel = m_channels[channelIdx];

		stream.Serialise(channel.m_numEffects);

		if(stream.GetDirection() == Stream::STREAM_IN)
		{
			channel.m_patternPages = new Channel::PatternPage[m_numPatternPages];
		}

		//Note pattern pages
		for(int patternPageIdx = 0; patternPageIdx < m_numPatternPages; patternPageIdx++)
		{
			Channel::PatternPage& patternPage = channel.m_patternPages[patternPageIdx];

			if(stream.GetDirection() == Stream::STREAM_IN)
			{
				patternPage.m_notes = new Channel::PatternPage::Note[m_numNoteRowsPerPattern];
			}

			//Notes
			for(int noteIdx = 0; noteIdx < m_numNoteRowsPerPattern; noteIdx++)
			{
				Channel::PatternPage::Note& note = patternPage.m_notes[noteIdx];

				stream.Serialise(note.m_note);
				stream.Serialise(note.m_octave);
				stream.Serialise(note.m_volume);

				//Note effects
				for(int effectIdx = 0; effectIdx < channel.m_numEffects; effectIdx++)
				{
					stream.Serialise(note.m_effects[effectIdx].m_effectType);
					stream.Serialise(note.m_effects[effectIdx].m_effectValue);
				}

				stream.Serialise(note.m_instrument);
			}
		}
	}

	//Samples
	stream.Serialise(m_numSamples);
	for(int i = 0; i < m_numSamples; i++)
	{
		stream.Serialise(m_samples[i]);
	}
}

void DMFFile::Instrument::Serialise(Stream& stream)
{
	//Instrument name
	stream.Serialise(m_name);

	//Instrument mode (FM/PSG)
	stream.Serialise(m_mode);

	if(m_mode == INSTRUMENT_PSG)
	{
		stream.Serialise(m_paramsPSG);
	}
	else if(m_mode == INSTRUMENT_FM)
	{
		stream.Serialise(m_paramsFM);
	}
}

void DMFFile::Instrument::ParamDataFM::Serialise(Stream& stream)
{
	//Common params
	stream.Serialise(alg);
	stream.Serialise(fb);
	stream.Serialise(lfo);
	stream.Serialise(lfo2);

	//Operators
	for(int i = 0; i < sMaxOperators; i++)
	{
		stream.Serialise(m_operators[i]);
	}
}

void DMFFile::Instrument::ParamDataFM::Operator::Serialise(Stream& stream)
{
	stream.Serialise(am);
	stream.Serialise(ar);
	stream.Serialise(dr);
	stream.Serialise(mul);
	stream.Serialise(rr);
	stream.Serialise(sl);
	stream.Serialise(tl);
	stream.Serialise(dt2);
	stream.Serialise(rs);
	stream.Serialise(dt);
	stream.Serialise(d2r);
	stream.Serialise(ssg);
}

void DMFFile::Instrument::ParamDataPSG::Serialise(Stream& stream)
{
	stream.Serialise(envelopeVolume);
	stream.Serialise(envelopeArpeggio);
	stream.Serialise(arpeggioMode);
	stream.Serialise(envelopeDutyNoise);
	stream.Serialise(envelopeWaveTable);
}

void DMFFile::Instrument::ParamDataPSG::Envelope::Serialise(Stream& stream)
{
	//Envelope size
	stream.Serialise(envelopeSize);

	//Envelope values
	envelopeData = new int32_t[envelopeSize];
	for(int i = 0; i < envelopeSize; i++)
	{
		stream.Serialise(envelopeData[i]);
	}

	//Loop position
	if(envelopeSize > 0)
	{
		stream.Serialise(loopPosition);
	}
}

void DMFFile::WaveTable::Serialise(Stream& stream)
{
	stream.Serialise(m_waveTableSize);

	if(stream.GetDirection() == Stream::STREAM_IN)
	{
		m_waveTableData = new uint32_t[m_waveTableSize];
	}

	for(int i = 0; i < m_waveTableSize; i++)
	{
		stream.Serialise(m_waveTableData[i]);
	}
}

void DMFFile::Sample::Serialise(Stream& stream)
{
	stream.Serialise(m_sampleSize);
	stream.Serialise(m_name);
	stream.Serialise(m_sampleRate);
	stream.Serialise(m_pitch);
	stream.Serialise(m_amplitude);
	stream.Serialise(m_bitsPerSample);

	if(stream.GetDirection() == Stream::STREAM_IN)
	{
		m_sampleData = new uint16_t[m_sampleSize];
	}

	for(int i = 0; i < m_sampleSize; i++)
	{
		stream.Serialise(m_sampleData[i]);
	}
}
