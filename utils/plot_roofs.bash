#!/bin/bash 
#################################################################################################################################
# This is a script to plot results output by main benchmark.                                                                    #
# usage: plot_roofs.sh -o <output> -f <format> -t <filter(for input info col)> -i <input>                                       #
#                                                                                                                               #
# Author: Nicolas Denoyelle (nicolas.denoyelle@inria.fr)                                                                        #
# Date: 12/11/2015 (FR format)                                                                                                  #
# Version: 0.1                                                                                                                  #
#################################################################################################################################

FORMATS=("gnuplot" "R")
usage(){
    printf "plot_roofs.sh -o <output> -f <format> -t <filter(for input info col)> -i <input>\n"; 
    printf "FORMATS: "; 
    for format in ${FORMATS[@]}; do
	printf "$format "
    done
    printf "\n"
    exit
}

FILTER="STORE|LOAD"

#################################################################################################################################
## Parse options
while getopts :o:i:t:f:h opt; do
    case $opt in
	o) OUTPUT=$OPTARG;;
	f) FORMAT=$OPTARG;;
	i) INPUT=$OPTARG;;
	t) FILTER="$OPTARG";;
	h) usage;;
	:) echo "Option -$OPTARG requires an argument."; exit;;
    esac
done

## Check arguments output if not
if [ -z "$OUTPUT" ]; then
    OUTPUT=$PWD/roofline_chart.pdf
fi

if [ -z "$FORMAT" ]; then
    FORMAT=gnuplot
fi

if [ -z "$INPUT" ]; then
    echo "Input file required"
    usage
fi

#################################################################################################################################
## output methods

output_gnuplot(){
    GBYTES_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"gbyte")){print i; exit}}}' $INPUT)
    OI_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"flops/byte")){print i; exit}}}' $INPUT)
    TYPE_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"info")){print i; exit}}}' $INPUT)
    OBJ_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"obj")){print i; exit}}}' $INPUT)
    GFLOPS_F=$(awk '{for(i=1; i<=NF; i++){if(match(tolower($i),"gflop")){print i; exit}}}' $INPUT)
    GFLOPS=$(awk -v f=$GFLOPS_F '{if(NR>1 && $f !=0){print $f; exit}}' $INPUT)
    gnuplot <<EOF
    set terminal pdfcairo enhanced size 10in, 5in
    #roofline function
    roofline(oi,b,f) = (b*oi <= f) ? (b*oi) : f
    set title 'roofline chart'
    set xlabel 'Flops/Byte'
    set xrange [2**-12:2**6]
    set ylabel 'GFlops/s'
    set yrange [:$GFLOPS*1.2]
    set logscale x 2
    set format x '2^{%L}'
    set logscale y 10
    set grid xtics, ytics, mytics
    set key bottom right
    set output '$OUTPUT'
    set autoscale fix
    plot $(cat $INPUT | grep -E "$FILTER" | awk -v gflops=$GFLOPS -v gf=$GFLOPS_F -v bf=$GBYTES_F -v oif=$OI_F -v of=$OBJ_F -v tf=$TYPE_F -v filter=$FILTER -v q="'" '
    BEGIN{
      linetype=0;
    } 
    {      
      if($gf==0 && $bf!=0){ #Its a line 
        linetype++;
        title = sprintf("%s\\_%s", $tf, $of);
        if(linetype == 1){ #first line
          printf "roofline(x,%.4f,%.4f) t %s%s%s w l lt %d", $bf, gflops, q, title, q, linetype;
        }
        else{
          printf ", roofline(x,%.4f,%.4f) t %s%s%s w l lt %d", $bf, gflops, q, title, q, linetype;
        }
      }
      else if($oif!=0 && $bf!=0){ #Its a point
        printf ", \"<echo %s%f %f%s\" w p pt %d lt %d notitle", q, $oif, $gf, q, linetype+1 , linetype
      }
    }')
EOF
}


output_R(){
    R --vanilla --silent --slave <<EOF
#Load data
d = read.table("$INPUT",header=TRUE)

#Retrieve Max Gflops and column specific ids
for (column in colnames(d)){
  if(grepl("GFlop",column,ignore.case=TRUE)){
    GFlops=max(d[,eval(quote(column))],na.rm=TRUE)
    flops_id = eval(quote(column))
  }
  if(grepl("info",column,ignore.case=TRUE)){
    type_id = eval(quote(column))
  }
  if(grepl("GByte",column,ignore.case=TRUE)){
    bandwidth_id = eval(quote(column))
  }
  if(grepl("obj",column,ignore.case=TRUE)){
    obj_id = eval(quote(column))
  }
  if(grepl("flops|byte",column,ignore.case=TRUE)){
    oi_id = eval(quote(column))
  }
}

#Defining scale
lseq <- function(from=1, to=100000, length.out = 6) {
  exp(seq(log(from), log(to), length.out = length.out))
}

# axes
xmin = 2^-12; xmax = 2^6; xlim = c(xmin,xmax)
xticks = lseq(xmin,xmax,log(xmax/xmin,base=2) + 1)
xlabels = sapply(xticks, function(i) as.expression(bquote(2^ .(round(log(i,base=2))))))

ymax = 10^ceiling(log10(GFlops)); ymin = ymax/100000; ylim = c(ymin,ymax)
yticks = lseq(ymin, ymax, log10(ymax/ymin))
ylabels = sapply(yticks, function(i) as.expression(bquote(10^ .(round(log10(i))))))

#plot points for bandwidth validation
plot_valid <- function(type, obj){
  valid = d[d[,obj_id] == obj & d[,type_id] == type & d[,flops_id] !=0, ]
  points(valid[,oi_id], valid[,flops_id], asp=1, pch=color, col=color)
  par(new=TRUE, ann=FALSE)
}

oi = lseq(xmin,xmax,500)
plot_bandwidths <- function(row) {
  color <<- color+1
  obj = row[obj_id]
  type = row[type_id]
  bandwidth=as.double(row[bandwidth_id])
  plot(oi, sapply(oi*bandwidth, min, GFlops), lty=1, type="l", log="xy", xlim=xlim, ylim=ylim, axes=FALSE, main="roofline chart", xlab="Flops/Byte", ylab="GFlops/s",  col=color, panel.first=abline(h=yticks, v=xticks,col = "darkgray", lty = 3))
  plot_valid(type,obj)
  name = paste(c(as.character(obj),"_",as.character(type)),collapse = '')
  caption <<- c(caption, name)
  par(new=TRUE, ann=FALSE)
}

#plot each bandwidth matching the required type pattern
bandwidth_rows = subset(d, d[,flops_id]==0 & grepl("$FILTER",d[,type_id]))
caption=c()
color=0
pdf("$OUTPUT", family = "Helvetica", title="roofline chart", width=10, height=5)
invisible(apply(bandwidth_rows, 1, plot_bandwidths))
  
#draw axes
axis(1, at=xticks, labels=xlabels)
axis(2, at=yticks, labels=ylabels)
box()
#draw legend
legend("bottomright", caption, cex=.7, lty=1, col=1:color)

#output
graphics.off()
EOF
}


#################################################################################################################################
## output
  
if [ "$FORMAT" = "gnuplot" ]; then
  output_gnuplot
elif [ "$FORMAT" = "R" ]; then
  output_R
fi

