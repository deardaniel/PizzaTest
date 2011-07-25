AVite - an ATK-based version of HVite

AVite is designed for evaluating speech recognition performance on
off-line test sets.  It uses the standard ATK ASource, ACode and ARec
components to set up a recognition processing pipeline.  Its main
purpose is to ensure that an ATK-based recognition system is giving
similar recognition performance to the equivalent off-line HTK-based
system.

AVite implements a subset of the functionality provided by HVite and
for most purposes it is plug-compatible.  The main limitations of
AVite compared to HVite are that the input must be waveform data, only
continuous-density HMMs are supported, forced alignments at the model
and state level are not supported and the output MLF formatting is
limited to the HTK default format (ie start, end, label, score) and
the options S (suppress score) and N (normalise scores per frame).  In
addition, AVite provides an output format flag F which causes the
normal log probability scores to be replaced by confidence scores (see
6.3.2).  AVite is invoked from the command-line in an identical
fashion to HVite, i.e.

AVite [options] VocabFile HMMList DataFiles...

where VocabFile is the name of a dictionary, HMMList is a list of HMMs
and DataFiles are the speech files to test, the latter are normally
listed in a .scp file input via the -S option.  The supported options
are

-i s	output transcriptions to MLF s
-g s	load n-gram language model from file s
-o s	output label formatting (F=conf scores, S=suppress, N=normalise)  
-p f	set inter-word transition penalty to f
-q f	n-gram language model scale factor
-r f	pronunciation scale factor
-s f	set link grammar scale factor to f
-t  f	set genereral pruning threshold to f
-u i	set the maximum number of active models to i
-v f	set word end pruning threshold to f
-w s	recognise from network defined in file s (recognition mode)
-A	print command line args
-C cf	load config file from cf
-D	display configuration variables 
-H mmf	load hmm macro file mmf  (NB at most 2 mmf files can be loaded)
-I mlf	load master label file (alignment mode)
-S f	set script file (.scp) to f
-T N	set tracing to N
-V      Print version information           Off
-X ext  Set input label (or net) file ext   lab

For normal operation, a recognition network must be supplied via the
-w option.  If the -w option is omitted and an mlf is supplied via the
-I option, then AVite operates in alignment mode.  Each speech file
must then have an entry in the mlf.  Prior to recognising the file,
this entry is used to construct a linear recognition network
consisting of the words in sequence.  Thus, every file is recognised
using its own recognition network and that network forces a word level
alignment.

In the absence of an N-gram language model, the results computed by
AVite should be very similar to those computed by HVite but they will
not be identical for the following reasons: a) AVite uses a special
version of HParm which has no "table mode".  When HParm computes delta
and delta-delta coefficients over a fixed utterance, it uses a
modified formula for the final few frames.  Since AVite only works in
"buffer mode", it uses an alternative strategy of padding the signal
with silence.  b) If the target parameterisation includes "_Z" then
cepstral mean normalisation is required.  HVite calculates the
cepstral mean on a per utterance basis by computing the mean over the
whole utterance and then subtracting it from every vector.  AVite uses
running average cepstral mean normalisation and resets the
running-mean back to the default at the start of every input
utterance.  Some adjustment of the time constant set by CMNTCONST may
be necessary to get satisfactory performance.  c) If an n-gram
language model is used, then there is no direct comparison with HTK
since the HTK decoder does not support n-grams.  Using a bi-gram
language model with a word loop in ATK should give similar results to
using a bigram network and no language model in both HTK and ATK.  d)
HTK uses a word-pair approximation when computing multiple hypotheses
(i.e. when NTOKS > 1).  ATK computes multiple hypotheses using a
trigram approximation.
 
