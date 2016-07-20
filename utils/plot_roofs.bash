#!/bin/bash 
#################################################################################################################################
# This is a script to plot results output by main benchmark.                                                                    #
# usage: plot_roofs.sh -o <output> -f <format> -t <filter(for input info col)> -i <input>                                       #
#                                                                                                                               #
# Author: Nicolas Denoyelle (nicolas.denoyelle@inria.fr)                                                                        #
# Date: 12/11/2015 (FR format)                                                                                                  #
# Version: 0.1                                                                                                                  #
#################################################################################################################################

usage(){
    printf "plot_roofs.sh -o <output> -f <filter(for input info col)> -i <input> -d <data-file> -t <title>\n"; 
    exit
}

FILTER="*"
TITLE="roofline chart"

#################################################################################################################################
## Parse options
while getopts :o:i:t:d:m:f:h opt; do
    case $opt in
	o) OUTPUT=$OPTARG;;
	d) DATA=$OPTARG;;
	m) METHOD=$OPTARG;;
	i) INPUT=$OPTARG;;
	f) FILTER="$OPTARG";;
	t) TITLE="$OPTARG";;
	h) usage;;
	:) echo "Option -$OPTARG requires an argument."; exit;;
    esac
done

## Check arguments output if not
if [ -z "$OUTPUT" ]; then
    OUTPUT=$PWD/roofline_chart.pdf
fi

if [ -z "$INPUT" ]; then
    echo "Input file required"
    usage
fi

output_R(){
    R --vanilla --silent --slave <<EOF
#Globals
#Load data
d = read.table("$INPUT",header=TRUE)

#Columns id
dobj       = 1;    #The obj column id
dbandwidth = 7;    #The bandwidth column id
dgflops    = 8;    #The gflop/s column id
doi        = 9;    #The oi column id
dinfo      = 11;   #The info column id

d = subset(d, grepl("$FILTER",d[,dinfo]))

fpeaks            = d[d[,dbandwidth]==0,] #The peak floating point performance
fpeak_max         = max(fpeaks[,dgflops]) #the top peak performance
bandwidths        = d[d[,dgflops]==0,]    #The bandwidths

#Logarithmic sequence of points
lseq <- function(from=1, to=100000, length.out = 6) {
  exp(seq(log(from), log(to), length.out = length.out))
}

# axes
xmin = 2^-8; xmax = 2^6; xlim = c(xmin,xmax)
xticks = lseq(xmin,xmax,log(xmax/xmin,base=2) + 1)
xlabels = sapply(xticks, function(i) as.expression(bquote(2^ .(round(log(i,base=2))))))
oi = lseq(xmin,xmax,500)

ymax = 10^ceiling(log10(fpeak_max)); ymin = ymax/10000; ylim = c(ymin,ymax)
yticks = lseq(ymin, ymax, log10(ymax/ymin))
ylabels = sapply(yticks, function(i) as.expression(bquote(10^ .(round(log10(i))))))

pdf("$OUTPUT", family = "Helvetica", title="roofline chart", width=10, height=5)

#plot bandwidths roofs
par(ann=FALSE)
plot_bandwidth <- function(bandwidth, color = 1){
  gflops = sapply(oi*bandwidth, min, fpeak_max)
  plot(oi, gflops, lty=1, type="l", log="xy", axes=FALSE, xlim=xlim, ylim=ylim, col=color, panel.first=abline(h=yticks, v=xticks,col = "darkgray", lty = 3))
  par(new=TRUE);
}
for(i in 1:nrow(bandwidths)){
  bandwidth = bandwidths[i,dbandwidth];
  plot_bandwidth(bandwidth, i);
}

#plot fpeak roofs
abline(h = fpeaks[,dgflops], lty=3, col=1, lwd=2);
axis(2, labels = fpeaks[,dinfo], at = fpeaks[,dgflops], las=1, tick=FALSE, pos=xmin*2, padj=0, hadj=0)

#plot validation points
plot_points <- function(df, col_start = 0){
  points       = subset(df, df[,dgflops]!=0 & df[,dbandwidth]!=0)
  points_info  = unique(points[,dinfo])
  points_objs  = unique(points[,dobj])

 for(i in 1:length(points_objs)){
    obj = points_objs[i]
    for(j in 1:length(points_info)){
      type = points_info[j]
      idx = col_start+(i-1)*length(points_info)+j
      valid = subset(points, points[,dinfo]==type & points[,dobj]==obj)
      points(valid[,doi], valid[,dgflops], asp=1, pch=idx, col=idx)
      par(new=TRUE);
    }
  }
  idx
}
plot_points(d)

#plot MISC points
if("$DATA" != ""){
  col_start = nrow(bandwidths)
  misc = read.table("$DATA",header=TRUE)
  misc = subset(misc, grepl("$FILTER",misc[,dinfo]))
  col_end = plot_points(misc, col_start = col_start)
  labels = unique(misc[,dinfo])
  range = col=col_start+1:col_end
  legend("topright", legend=labels, cex=.7, lty=1, col=range, pch=range)
}

#draw axes, title and legend
axis(1, at=xticks, labels=xlabels)
axis(2, at=yticks, labels=ylabels)
title(main = "$TITLE", xlab="Flops/Byte", ylab="GFlops/s")
legend("bottomright", legend=paste(bandwidths[,dobj], bandwidths[,dinfo], sep=" "), cex=.7, lty=1, col=1:nrow(bandwidths))
box()

#output
graphics.off()
EOF
}


#################################################################################################################################
## output
output_R

