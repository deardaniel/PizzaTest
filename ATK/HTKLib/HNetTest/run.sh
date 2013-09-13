wprog=Debug/HNetTest
uprog=HNetTest
hmms=../../Resources/UK_SI_ZMFCC/WI4
list=../../Resources/UK_SI_ZMFCC/hmmlist

if [ -f $wprog ]; then
  prog=$wprog
elif [ -f $uprog ]; then
  prog=$uprog
else
  echo "Cant find HNetTest program"
  exit 1
fi
echo $prog -C bitbut.cfg -H $hmms bitbut.dct bitbut.net $list