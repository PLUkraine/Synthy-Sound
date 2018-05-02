#include <algorithm>
#include <vector>
#include <map>

#include <stdlib.h>
#include <math.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

// Hertz to Angular
#define H2W(hertz) (hertz)*2*(float)M_PI
// "Safe" pointer delete
#define DELETE_PTR(x) delete (x); (x) = nullptr

const int AMPLITUDE = 20000;
const int SAMPLE_RATE = 44100;

class EnvelopeADSR
{
public:
    float attackTime;
    float decayTime;
    float releaseTime;
    
    float sustainAmplitude;
    float startAmplitude;
    
    EnvelopeADSR()
    {
        attackTime = 0.01f;
        decayTime = 1.0f;
        startAmplitude = 1.0f;
        sustainAmplitude = 0.0f;
        releaseTime = 1.0f;
    }
    
    float getAmplitude(float t, float timeOn, float timeOff)
    {
        float amplitude = 0.0f;
        
        float lifeTime = t - timeOn;
        if (timeOn > timeOff)
        {
            // ADS
            // Attack
            if (lifeTime <= attackTime){
                amplitude = (lifeTime / attackTime) * startAmplitude;
            }
            // Decay
            else if (lifeTime <= attackTime + decayTime) {
                amplitude = (lifeTime - attackTime) / decayTime * (sustainAmplitude - startAmplitude) + startAmplitude;
            }
            // Sustain
            else {
                amplitude = sustainAmplitude;
            }
        }
        else {
            // Release
            lifeTime = timeOff - timeOn;
            float releaseAmplitude = 0.0f;
            
            if (lifeTime <= attackTime) {
                releaseAmplitude = (lifeTime / attackTime) * startAmplitude;
            }
            else if (lifeTime <= (attackTime + decayTime)) {
                releaseAmplitude = ((lifeTime - attackTime) / decayTime) * (sustainAmplitude - startAmplitude) + startAmplitude;
            }
            if (lifeTime > (attackTime + decayTime)) {
                releaseAmplitude = sustainAmplitude;
            }
            amplitude = ((t - timeOff) / releaseTime) * (0.0 - releaseAmplitude) + releaseAmplitude;
        }
        
        if (amplitude < 0.0000f) {
            amplitude = 0.0f;
        }
        
        return amplitude;
    }
    
};

enum class WaveType
{
    SINE, SQUARE, TRIANGLE, SAW, NOISE
};

// oscilator function (with Frequency Modulation, FM)
float getWave(WaveType wave_type, float t, float hertz, float fmAmplitude=0, float fmHertz=0)
{
    float answer = 0.0f;
    float freq = H2W(hertz) * t + fmAmplitude * hertz * sinf(H2W(fmHertz) * t);
    switch (wave_type) {
        case WaveType::SINE:
            return sinf(freq);
            break;
        case WaveType::SQUARE:
            return sinf(freq) > 0 ? 1 : -1;
            break;
        case WaveType::TRIANGLE:
            return asinf(sinf(freq)) * 2.0f / (float)M_PI;
            break;
        case WaveType::SAW:
            answer = 0.0f;
            for (int i=1; i<40; ++i) {
                answer += sinf((float)i * freq) / (float)i;
            }
            return answer * 2.0f / (float)M_PI;
            break;
        case WaveType::NOISE:
            return 2.0f * (float)rand()/(float)RAND_MAX - 1.0;
            break;
    }
}

class Instrument
{
public:
    float volume;
    EnvelopeADSR envelope;
    
    Instrument()
    {
        volume = 1.0;
    }
    virtual ~Instrument() {}
    
    virtual float sound(float hertz, float t, float timeOn, float timeOff, bool &noteIsAlive)=0;
};

class Bell : public Instrument
{
public:
    Bell()
    {
        envelope.attackTime = 0.01f;
        envelope.decayTime = 1.0f;
        envelope.startAmplitude = 1.0f;
        envelope.sustainAmplitude = 0.0f;
        envelope.releaseTime = 1.0f;
    }
    
    float sound(float hertz, float t, float timeOn, float timeOff, bool &noteIsAlive)
    {
        float amplitude = envelope.getAmplitude(t, timeOn, timeOff);
        noteIsAlive = amplitude > 0.0f;
        return volume * amplitude * (
                                    + 1.0f * getWave(WaveType::SINE, t, hertz * 2.0f, 0.001f, 5.0f)
                                    + 0.5f * getWave(WaveType::SINE, t, hertz * 3.0f)
                                    + 0.25f * getWave(WaveType::SINE, t, hertz * 4.0f));
    }
};
class Harmonica : public Instrument
{
public:
    Harmonica()
    {
        envelope.attackTime = 0.1f;
        envelope.decayTime = 0.01f;
        envelope.startAmplitude = 1.0f;
        envelope.sustainAmplitude = 0.8f;
        envelope.releaseTime = 0.1f;
    }
    
    float sound(float hertz, float t, float timeOn, float timeOff, bool &noteIsAlive)
    {
        float amplitude = envelope.getAmplitude(t, timeOn, timeOff);
        noteIsAlive = amplitude > 0.0f;
        return volume * amplitude * (
                                     + 1.0f * getWave(WaveType::SQUARE, t, hertz, 0.001f, 5.0f)
                                     + 0.5f * getWave(WaveType::SQUARE, t, hertz * 1.5f)
                                     + 0.25f * getWave(WaveType::SQUARE, t, hertz * 2.0f)
                                     + 0.05f * getWave(WaveType::NOISE, t, 0.0f));
    }
};
class PureSaw : public Instrument
{
public:
    PureSaw()
    {
        volume = 0.8;
        envelope.attackTime = 0.01f;
        envelope.decayTime = 0.01f;
        envelope.startAmplitude = 1.0f;
        envelope.sustainAmplitude = 0.8f;
        envelope.releaseTime = 0.01f;
    }
    
    float sound(float hertz, float t, float timeOn, float timeOff, bool &noteIsAlive)
    {
        float amplitude = envelope.getAmplitude(t, timeOn, timeOff);
        noteIsAlive = amplitude > 0.0f;
        return volume * amplitude * getWave(WaveType::SAW, t, hertz, 0.001f, 5.0f);
    }
};

struct Note
{
    int id;
    float freq;
    float timeOn;
    float timeOff;
    bool active;
    Instrument *instrument;
    
    Note()
    {
        id = 0;
        freq = 0.0f;
        timeOn = 0.0f;
        timeOff = 0.0f;
        active = false;
        instrument = nullptr;
    }
};

// custom data structure, passed inside the audio callback
typedef struct
{
    int sample_nr = 0;
    std::vector<Note> notes;
} AudioCustomData;


// audio callback, it is responcible for the audio samples generation
void audio_callback(void *user_data, Uint8 *raw_buffer, int bytes)
{
    Sint16 *buffer = (Sint16*)raw_buffer;
    int length = bytes/2; // 2 bytes per sample for AUDIO_S16SYS
    AudioCustomData *data = (AudioCustomData *)user_data;
    
    // mix all the notes!
    for (int i = 0; i < length; ++i, data->sample_nr++)
    {
        float time = (float)data->sample_nr / (float)SAMPLE_RATE;
        buffer[i] = 0;
        for (Note &note : data->notes)
        {
            bool alive;
            float result = note.instrument->sound(note.freq, time, note.timeOn, note.timeOff, alive);
            note.active = alive;
            buffer[i] += (Sint16)(AMPLITUDE/4 * result);
        }
    }
    // if no notes available, we still are required to provide samples!
    if (data->notes.empty()) {
        for (int i = 0; i < length; data->sample_nr++) {
            buffer[i++] = (Sint16)(0.0f);
        }
    }
    
}

void initializeKeyMap(std::map<SDL_Scancode, Note> &key_to_note, Instrument *instrument)
{
    Note note;
    note.instrument = instrument;
    
    // A3
    note.id = 0;
    note.freq = 440.0f * powf(2, -12.f/12);
    key_to_note[SDL_SCANCODE_Z] = note;
    // A3#
    note.id++;
    note.freq = 440.0f * powf(2, -11.f/12);
    key_to_note[SDL_SCANCODE_S] = note;
    // B3
    note.id++;
    note.freq = 440.0f * powf(2, -10.f/12);
    key_to_note[SDL_SCANCODE_X] = note;
    // C4
    note.id++;
    note.freq = 440.0f * powf(2, -9.f/12);
    key_to_note[SDL_SCANCODE_C] = note;
    // C4#
    note.id++;
    note.freq = 440.0f * powf(2, -8.f/12);
    key_to_note[SDL_SCANCODE_F] = note;
    // D4
    note.id++;
    note.freq = 440.0f * powf(2, -7.f/12);
    key_to_note[SDL_SCANCODE_V] = note;
    // D4#
    note.id++;
    note.freq = 440.0f * powf(2, -6.f/12);
    key_to_note[SDL_SCANCODE_G] = note;
    // E4
    note.id++;
    note.freq = 440.0f * powf(2, -5.f/12);
    key_to_note[SDL_SCANCODE_B] = note;
    // E4#
    note.id++;
    note.freq = 440.0f * powf(2, -4.f/12);
    key_to_note[SDL_SCANCODE_N] = note;
    // F4
    note.id++;
    note.freq = 440.0f * powf(2, -3.f/12);
    key_to_note[SDL_SCANCODE_J] = note;
    // F4#
    note.id++;
    note.freq = 440.0f * powf(2, -2.f/12);
    key_to_note[SDL_SCANCODE_M] = note;
    // G4
    note.id++;
    note.freq = 440.0f * powf(2, -1.f/12);
    key_to_note[SDL_SCANCODE_K] = note;
    // G4#
    note.id++;
    note.freq = 440.0f * powf(2, 0.f/12);
    key_to_note[SDL_SCANCODE_COMMA] = note;
}


int main(int argc, char* args[])
{
    if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_EVENTS) == -1)
    {
        printf("Error initializing SDL...\n");
        return 1;
    }
    
    // map keyboard to notes
    std::map<SDL_Scancode, Note> key_to_note;
    Bell bell; Harmonica harmonica;
    initializeKeyMap(key_to_note, &bell);
    
    
    // video
    SDL_Window *screen = SDL_CreateWindow("Synthetic Soundy",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          640, 480,
                                          SDL_WINDOW_OPENGL);
    // audio
    AudioCustomData custom_data;
    
    SDL_AudioSpec want;
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_callback;
    want.userdata = &custom_data;
    
    SDL_AudioSpec have;
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (audio_device == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to open audio: %s", SDL_GetError());
    }
    if (want.format != have.format) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to get desired AudioSpec");
    }
    
    SDL_PauseAudioDevice(audio_device, 0);
    SDL_Event event;
    bool quit = false;
    while(!quit)
    {
        SDL_LockAudioDevice(audio_device);
        while(SDL_PollEvent(&event))
        {
            float sound_time = (float)custom_data.sample_nr / (float)SAMPLE_RATE;
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            // key was pressed
            else if (event.type == SDL_KEYDOWN && !event.key.repeat)
            {
                // check if such key is mapped to a note
                SDL_Scancode scancode = event.key.keysym.scancode;
                auto it = key_to_note.find(scancode);
                // if yes, push it in the vector
                if (it != key_to_note.end()) {
                    Note note = it->second;
                    note.timeOn = sound_time;
                    note.active = true;
                    custom_data.notes.push_back(note);
                }
            }
            // key was released
            else if (event.type == SDL_KEYUP) {
                SDL_Scancode scancode = event.key.keysym.scancode;
                auto it = key_to_note.find(scancode);
                if (it != key_to_note.end()) {
                    int note_id = it->second.id;
                    for (Note &n : custom_data.notes) {
                        if (n.id == note_id) {
                            n.timeOff = sound_time;
                        }
                    }
                }
            }
        }
        // remove non-active notes from vector
        custom_data.notes.erase(std::remove_if(custom_data.notes.begin(), custom_data.notes.end(),
                                               [](const Note& n){ return !n.active;}),
                                custom_data.notes.end());
        SDL_UnlockAudioDevice(audio_device);
        
        SDL_Delay(1000/30);
    }

    SDL_DestroyWindow(screen);
    SDL_CloseAudioDevice(audio_device);
    SDL_Quit();

    return 0;
}
