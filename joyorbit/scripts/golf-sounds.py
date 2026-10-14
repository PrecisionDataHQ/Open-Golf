"""Generate original MIT-licensed replacement golf sound effects, without source audio."""
from pathlib import Path
import argparse, math, os, random, struct, subprocess, wave
parser=argparse.ArgumentParser();parser.add_argument('destination',type=Path);args=parser.parse_args()
args.destination.mkdir(parents=True,exist_ok=True)
specs={
 'footstep_grass_004':(.12,130,.55), 'impactPlank_medium_000':(.18,210,.65),
 'drop_001':(.12,680,.15), 'drop_003':(.09,880,.1),
 'confirmation_002':(.28,660,.02), 'error_008':(.22,180,.05),
 'in_water':(.3,320,.85),
}
for index,(name,(duration,frequency,noise)) in enumerate(specs.items()):
    rng=random.Random(1734+index);rate=44100;samples=[]
    for frame in range(int(duration*rate)):
        t=frame/rate;envelope=min(1,t/.005)*math.exp(-t/(duration*.3))
        tone=math.sin(2*math.pi*frequency*t*(1-.3*t/duration))
        if name=='confirmation_002':tone=.6*math.sin(2*math.pi*frequency*t)+.4*math.sin(2*math.pi*frequency*1.5*t)
        value=.3*envelope*((1-noise)*tone+noise*rng.uniform(-1,1))
        samples.append(struct.pack('<h',int(max(-1,min(1,value))*32767)))
    source=args.destination/(name+'.wav')
    with wave.open(str(source),'wb') as output:
        output.setnchannels(1);output.setsampwidth(2);output.setframerate(rate);output.writeframes(b''.join(samples))
    subprocess.run([os.environ.get('JOYORBIT_OGGENC','oggenc'),'-Q','-q','3','-s',str(1734+index),'-o',str(args.destination/(name+'.ogg')),str(source)],check=True)
