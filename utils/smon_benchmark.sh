#!/bin/sh 
#################################################################################################################################
# This is a script to run the benchmark with smon profiler, and output measured bandwidth instead of counted bandwidth.         #
# usage: smon_benchmark.sh -o <output> -b <benchmark_runnable>                                                                  #
#                                                                                                                               #
# Author: Nicolas Denoyelle (nicolas.denoyelle@inria.fr)                                                                        #
# Date: 12/11/2015 (FR format)                                                                                                  #
# Version: 0.1                                                                                                                  #
#################################################################################################################################

#################################################################################################################################
## Parse options
if [ -z $1 ]; then
    echo "$0 -o <output> -b <benchmark_runnable>"
    exit
fi

getopts :o:b:h opt
case $opt in
    o) OUTPUT=$OPTARG;;
    b) BENCHMARK_BIN=$OPTARG;;
    h) echo "$0 -o <output> -b <benchmark_runnable>"; exit;;
    :) echo "Option -$OPTARG requires an argument."; exit;;
    \?) echo "Invalid option -$OPTARG."; exit;;
esac

#################################################################################################################################
## Set output if not
if [ -z "$OUTPUT" ]; then
    OUTPUT=$PWD/../$(hostname)/benchmark.out
    if [ ! -d "../$(hostname)" ]; then
	mkdir ../$(hostname)
    fi
fi

#################################################################################################################################
## Check flags in /proc/cpuinfo
proc_has_flag(){
    awk -v flag=$1 /^flags[[:blank:]]*:/'{print match($0,flag)!=0; exit}' /proc/cpuinfo
}

CPU_FREQ=$(grep -m 1 "GHz" /proc/cpuinfo | tr -d GHz | awk '{print $10}')

#Check roof instruction type.
SSE=$(proc_has_flag "sse")
SSE2=$(proc_has_flag "sse2")
AVX=$(proc_has_flag "avx")
if [ $AVX -eq 1 ]; then
    FLOP_INS=4
    BYTE_INS=32
    TYPE=AVX
elif [ $SSE2 -eq 1 ]; then
    FLOP_INS=2
    BYTE_INS=16
    TYPE=SSE2
elif [ $SSE -eq 1 ]; then
    FLOP_INS=2
    BYTE_INS=16
    TYPE=SSE
else
    FLOP_INS=1
    BYTE_INS=8
    TYPE=SSE
fi


#################################################################################################################################
## Prepare smon to monitor the right events.
echo "Configuring smon ..."
SMON=smon
SMON_TAG=EVSET_ROOFLINE_BENCHMARK
SMON_OUT=smon.out

## Extract id from smon event_name
get_smon_evid(){
    smon_evid=$($SMON event -l | awk /$1/'{print $1}' | tr -d '[]')
    if [ -z "$smon_evid" ]; then
	echo "Cannot find smon event $1. Check its existency with smon."
	exit
    fi
    echo $smon_evid
}

## create event set if it does not already exists
if [ -z $(smon evset -l | awk /$SMON_TAG/'{print $1; exit}') ]; then
    ## Append smon event id for memory operations to the list of event id
    EVID="$(get_smon_evid MEM_UOP_RETIRED_ALL_LOADS):$(get_smon_evid MEM_UOP_RETIRED_ALL_STORES)"
    
    ## Append smon event id for floating point operations to the list of event id
    ## Also set multipliers while we chack type of flag.
    if [ $AVX -eq 1 ]; then
	EVID="$EVID:$(get_smon_evid FP_256_PACKED_DOUBLE):$(get_smon_evid FP_256_PACKED_SINGLE)"
    elif [ $SSE2 -eq 1 ]; then
smo	EVID="$EVID:$(get_smon_evid FP_SSE_PACKED_DOUBLE):$(get_smon_evid FP_SSE_PACKED_SINGLE)"
    elif [ $SSE -eq 1 ]; then
	EVID="$EVID:$(get_smon_evid FP_SSE_SCALAR_DOUBLE):$(get_smon_evid FP_SSE_SCALAR_SINGLE)"
    else
	echo "No suitable floating point flag was found among \"sse, sse2, avx\""
	exit
    fi
    $SMON evset -a tag=$SMON_TAG,events=$EVID   
fi

## Retrieve evset id.
EVSETID=$($SMON evset -l | tac | awk /$SMON_TAG/'{print $1; exit}' | tr -d '[]')


#################################################################################################################################
#Run with smon

BENCHMARK_BIN_OUT=$PWD/benchmark_standalone.out
## Create script to run binary with smon. smon doesn't allow binary to use arguments so we have to wrap it into a bash script.
BIN_SCRIPT=$PWD/user_bin_launcher.sh
echo "#!/bin/sh" > $BIN_SCRIPT
echo "$BENCHMARK_BIN --output $BENCHMARK_BIN_OUT" >> $BIN_SCRIPT
chmod +x $BIN_SCRIPT

# Start runnable with smon
echo "Start monitoring $BENCHMARK_BIN ..."
#$SMON profile -e $EVSETID -o $SMON_OUT -i -t 1 $BIN_SCRIPT

#remove script
rm $BIN_SCRIPT

#remove BENCHMARK_BIN_OUT header
awk '{if(NR>1){print}}' $BENCHMARK_BIN_OUT > $BENCHMARK_BIN_OUT.pre

#################################################################################################################################
# Filter, sort and parse results

echo "Sorting smon output ..."
awk /PMC/'{printf "%d %d %d %d\n", $2, $3, $10+$11, $12+$13}' $SMON_OUT | sort -k 1 > $SMON_OUT.sort
echo "Parsing smon output ..."
# parse smon output to get the number of mem and fp_ops instructions
smon_parse_ts(){
    mem_fp=$(awk -v ts=$ts_start  -v te=$ts_end /PMC/'
BEGIN{
  mem=0;
  fp=0;
}
{
  if($1>=ts && $2<=te){
    mem+=$3;
    fp+=$4;
  }
  if($1>te){
    exit;
  }
}
END{
  printf "%d %d", mem, fp
}' $SMON_OUT.sort)
}


#grep fields indexes
head -n 1 $BENCHMARK_BIN_OUT > $BENCHMARK_BIN_OUT.head
OBJ_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"obj")){print i; exit}}}' $BENCHMARK_BIN_OUT.head)
INFO_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"info")){print i; exit}}}' $BENCHMARK_BIN_OUT.head)
START_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"start")){print i; exit}}}' $BENCHMARK_BIN_OUT.head)
END_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"end")){print i; exit}}}' $BENCHMARK_BIN_OUT.head)
REPEAT_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"repeat")){print i; exit}}}' $BENCHMARK_BIN_OUT.head)
rm $BENCHMARK_BIN_OUT.head

# print header to final output
head -n 1 $BENCHMARK_BIN_OUT | tee $OUTPUT

#read OUTPUT line by line
while read line; do
    obj=$(echo $line | awk -v f=$OBJ_F    '{print $f}');
    ts_start=$(echo $line | awk -v f=$START_F  '{print $f}');
    ts_end=$(echo $line | awk -v f=$END_F    '{print $f}');
    cycles=$(awk -v e=$ts_end -v s=$ts_start 'BEGIN{print e-s}')
    append=$(echo $line | cut -d ' ' -f 11- )
    smon_parse_ts
    instructions=$(echo $mem_fp | awk '{print $1+$2}')
    bytes=$(echo $mem_fp | awk -v bytes_per_ins=$BYTE_INS '{print $1*bytes_per_ins}')
    flops=$(echo $mem_fp | awk -v flops_per_ins=$FLOP_INS '{print $2*flops_per_ins}')
    throughput=$(awk -v ins=$instructions -v cyc=$cycles 'BEGIN{print ins/cyc}')
    bandwidth=$(awk -v bytes=$bytes -v cyc=$cycles 'BEGIN{print bytes/cyc}')
    perf=$(awk -v flops=$flops -v cyc=$cycles 'BEGIN{print flops/cyc}')
    oi=$(awk -v flops=$flops -v bytes=$bytes 'BEGIN{print flops/bytes}')
    
    printf "%12s %16u %16u %16u %16u %16u %10f %10f %10f %10f  %s\n" \
	$obj $ts_start $ts_end $instructions $bytes $flops $throughput $bandwidth $perf $oi "$append"\
        | tee -a $OUTPUT
done < $BENCHMARK_BIN_OUT.pre

## Cleanup
#rm $BENCHMARK_BIN_OUT $BENCHMARK_BIN_OUT.pre $SMON_OUT.sort $SMON_OUT

## Print output info
echo "Output with smon written to: $OUTPUT"

