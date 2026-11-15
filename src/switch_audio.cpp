// SDL owns the audio renderer; Borealis UI sounds must not open a second renderer.
#include <borealis/platforms/switch/switch_audio.hpp>
namespace brls {
SwitchAudioPlayer::SwitchAudioPlayer() {}
SwitchAudioPlayer::~SwitchAudioPlayer() {}
bool SwitchAudioPlayer::load(Sound) { return true; }
bool SwitchAudioPlayer::play(Sound) { return true; }
}
