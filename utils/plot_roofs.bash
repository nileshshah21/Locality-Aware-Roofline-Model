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
    printf "plot_roofs.sh <options...>\n\n"
    printf "options:\n"
    printf "\t-o <output file(pdf)>\n"
    printf "\t-f <perl regular expression to filter input rows. (Match is done on info column)> \n"
    printf "\t-b (if several bandwidths matches on a resource, the best only is kept)\n"
    printf "\t-i <input file given by roofline utility>\n"
    printf "\t-d <input data-file given by roofline library to plot applications on the roofline chart>\n"
    printf "\t-t <plot title>\n"
    printf "\t-p (per_thread: divide roofs' value by the number of threads)\n"
    printf "\t-v (plot validation points if any)\n"
    printf "\t-s (plot bandwidths deviation around median)\n" 
    exit
}

FILTER=".*"
TITLE="roofline chart"
BEST="FALSE"
SINGLE="FALSE"
VALIDATION="FALSE"
DEVIATION="FALSE"
#################################################################################################################################
## Parse options
while getopts :o:i:t:d:m:f:hbpvs opt; do
    case $opt in
	o) OUTPUT=$OPTARG;;
	d) DATA=$OPTARG;;
	m) METHOD=$OPTARG;;
	i) INPUT=$OPTARG;;
	f) FILTER="$OPTARG";;
	t) TITLE="$OPTARG";;
	b) BEST="TRUE";;
	p) SINGLE="TRUE";;
	v) VALIDATION="TRUE";;
	s) DEVIATION="TRUE";;
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
#Columns id
dobj        = 1;    #The obj column id
dthroughput = 5;    #The instruction throughput
ddev        = 6;    #The instruction throughput standard deviation
dbandwidth  = 7;    #The bandwidth column id
dgflops     = 8;    #The gflop/s column id
doi         = 9;    #The oi column id
dthreads    = 10;   #The number of threads
dinfo       = 11;   #The info column id

filter <- function(df){
  subset(df, grepl("$FILTER", df[,dinfo], perl=TRUE))
}

#load data
d = filter(read.table("$INPUT",header=TRUE))

fpeaks            = d[d[,dbandwidth]==0,] #The peak floating point performance
bandwidths        = d[d[,dgflops]==0,]    #The bandwidths

if($BEST){
   top_bandwidth <- function(obj){
      max_bdw = max(bandwidths[bandwidths[,dobj]==obj, dbandwidth])
      which(bandwidths[,dbandwidth] == max_bdw)
   }
   bandwidths = bandwidths[unlist(unique(sapply(unique(bandwidths[,dobj]), top_bandwidth))),]
}
if($SINGLE){
  fpeaks[,dgflops] = fpeaks[,dgflops]/fpeaks[,dthreads]
  bandwidths[,dbandwidth] = bandwidths[,dbandwidth]/bandwidths[,dthreads]
}

fpeak_max         = max(fpeaks[,dgflops]) #the top peak performance

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
plot_bandwidth <- function(bandwidth, bytes_dev = 0, color = 1){
  gflops     = sapply(oi*bandwidth, min, fpeak_max)
  plot(oi, gflops, lty=1, type="l", log="xy", axes=FALSE, xlim=xlim, ylim=ylim, col=color, panel.first=abline(h=yticks, v=xticks,col = "darkgray", lty = 3))
  par(new=TRUE);
  if($DEVIATION){
    xdev=dev*0.5*sqrt(1+1/(bandwidth*bandwidth))
    coord.x = c(xmin*(1-xdev), fpeak_max/bandwidth*(1 - xdev), fpeak_max/bandwidth*(1 + xdev), xmin*(1+xdev))
    coord.y = c(xmin*bandwidth, fpeak_max, fpeak_max, xmin*bandwidth)
    polygon(coord.x,coord.y,col=adjustcolor(i,alpha.f=.25), lty="blank")
    par(new=TRUE);
  }
}
for(i in 1:nrow(bandwidths)){
  bandwidth = bandwidths[i,dbandwidth];
  dev = bandwidths[i,ddev]/bandwidths[i,dthroughput];
  plot_bandwidth(bandwidth, bytes_dev = dev, col=i);
}

#plot fpeak roofs
abline(h = fpeaks[,dgflops], lty=3, col=1, lwd=2);
yticks = c(yticks, fpeaks[,dgflops])
ylabels = c(ylabels, sprintf("%.2f", fpeaks[,dgflops]))
axis(2, labels = fpeaks[,dinfo], at = fpeaks[,dgflops], las=1, tick=FALSE, pos=xmin, padj=0, hadj=0)

#plot validation points
if($VALIDATION){
  points = subset(d, d[,dgflops]!=0 & d[,dbandwidth]!=0)
  if($BEST){
    #keep only for best rooflines
    points = merge(x = points, y=bandwidths[,c(dobj,dinfo)], by.x=c(dobj,dinfo), by.y=c(1,2))[, union(names(points), names(bandwidths[,c(dobj,dinfo)]))]
  }
  for(i in 1:nrow(bandwidths)){
    valid = subset(points, points[,dinfo]==bandwidths[i,dinfo] & points[,dobj]==bandwidths[i,dobj])
    if($SINGLE){
      valid[,dgflops] = valid[,dgflops]/valid[,dthreads]
    }
    points(valid[,doi], valid[,dgflops], asp=1, pch=i, col=i)
    par(new=TRUE);
  }
}

#plot MISC points
if("$DATA" != ""){
  misc = filter(read.table("$DATA",header=TRUE))
  types = unique(misc[,dinfo])
  range = nrow(bandwidths):nrow(bandwidths)+length(types)
  for(i in 1:length(types)){
    points = subset(misc, misc[,dinfo]==types[i])
    if($SINGLE){
      points[,dgflops] = points[,dgflops]/points[,dthreads]
    }
    points(points[,doi], points[,dgflops], asp=1, pch=i, col=i)
    par(new=TRUE);
  }
  legend("topright", legend=types, cex=.7, lty=1, col=1:length(types), pch=1:length(types))
}

#draw axes, title and legend
axis(1, at=xticks, labels=xlabels)
axis(2, at=yticks, labels=ylabels, las=1)
title(main = "$TITLE", xlab="Flops/Byte", ylab="GFlops/s")
legend("bottomright", legend=paste(bandwidths[,dobj], paste(bandwidths[,dinfo], sprintf("%.2f", bandwidths[,dbandwidth]), sep="="), "GB/s", sep=" "), cex=.7, lty=1, col=1:nrow(bandwidths))
box()

#output
graphics.off()
EOF
}


#################################################################################################################################
## output
output_R

