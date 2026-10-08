#include "particle_track.hpp"
#include <stdexcept>
#include <iostream>
#include <limits>
void check(bool v) { if (!v) throw std::runtime_error("particle check failed"); }
int main() {
    using namespace theater_particle;
    particle_track::Scheduler s;
    Command c; c.sequence=1; c.emitter_id=42; c.effect_id=3001;
    c.replay_time_ns=1'000'000'000; c.duration_seconds=.5f; c.repeat_seconds=2.f;
    check(s.add({c,true})); check(!s.add({c,true}));
    auto commands=s.evaluate(0); check(commands.size()==1 && commands[0].kind==CommandKind::clear);
    auto last=commands[0].sequence;
    commands=s.evaluate(1'100'000'000); check(commands.size()==1 && commands[0].kind==CommandKind::spawn && valid(commands[0],last)); last=commands[0].sequence;
    check(s.evaluate(1'200'000'000).empty());
    check(s.evaluate(1'200'000'000).empty()); // paused evaluation does not restart effects
    commands=s.evaluate(1'500'000'000); check(commands.size()==1 && commands[0].kind==CommandKind::remove && valid(commands[0],last)); last=commands[0].sequence;
    check(s.evaluate(3'100'000'000).empty());
    commands=s.evaluate(3'600'000'000); check(commands.size()==1 && commands[0].kind==CommandKind::spawn && valid(commands[0],last)); last=commands[0].sequence;
    s.seek(1'100'000'000); commands=s.evaluate(1'100'000'000);
    check(commands.size()==2 && commands[0].kind==CommandKind::clear && commands[1].kind==CommandKind::spawn);
    for(const auto& command:commands) {check(valid(command,last));last=command.sequence;}
    check(s.remove(42)); commands=s.evaluate(1'100'000'000);check(commands.size()==1 && commands[0].kind==CommandKind::remove);
    c.effect_kind=static_cast<EffectKind>(99);check(!valid(c));
    c.effect_kind=EffectKind::ffx_id;c.reserved=1;check(!valid(c));c.reserved=0;
    c.rotation={0,0,0,0};check(!valid(c));c.rotation={0,0,0,1};
    c.duration_seconds=std::numeric_limits<float>::infinity();check(!valid(c));
    std::cout<<"Particle lifetime, paused evaluation, repeat delay, seek cleanup and protocol validation PASS\n";
}
