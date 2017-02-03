#!/usr/bin/Rscript --vanilla --slave
#################################################################################################################################
# This is a script to plot results output by main benchmark.                                                                    #
#                                                                                                                               #
# Author: Nicolas Denoyelle (nicolas.denoyelle@inria.fr)                                                                        #
# Date: 12/11/2015 (FR format)                                                                                                  #
# Version: 0.1                                                                                                                  #
#################################################################################################################################

options("width"=system(command = "tput cols", intern = T))

#################################################################################################################################
## Parse options
#################################################################################################################################

library("optparse")

##Parse options
inOpt = make_option(opt_str = c("-i", "--input"), type = "character",  default = NULL, 
                    help = "Input plateform evaluation obtained with roofline binary.")
outOpt = make_option(opt_str = c("-o", "--output"), type = "character", default = "roofline_chart.pdf", 
                     help = "Output pdf chart.")
datOpt = make_option(opt_str = c("-d", "--data"), type = "character", default = NULL, 
                     help = "Use an application trace obtained with librfsampling to plot into the roofline chart.
                Each pair(type,info) is plot as a single point even if it appears several times. 
                In the latter case, it is possible to display the deviation with -s option.
                Points are represented as a transparent circle which radius is set according to the point median runtime")

dfilterOpt = make_option(opt_str = c("-f", "--filter"), type = "character", default = NULL, 
                     help = "Filter the trace obtained with librfsampling on columns type and info.
                Option argument is a comma separated list of items.
                For instance: load|ddot,store|ddot")
validOpt = make_option(opt_str = c("-v", "--validation"), type = "logical", default=FALSE, action="store_true",
                      help = "Plot validation points acquired in the input file on the chart.")
titleOpt = make_option(opt_str = c("-t", "--title"), type = "character", default = NULL,
                       help = "Plot title")
locationOpt = make_option(opt_str = c("-l", "--location"), type = "character", default = NULL,
                          help = "Select a specific location, contained in the input. (Filter on Obj column")
bandwidthOpt = make_option(opt_str = c("-b", "--bandwidth"), type = "character", default = NULL,
                           help = "Refine plot to keep only certain bandwidths. The option argument is a comma seperated list items.
                An item as the following syntax: 'memory|type'.
                For instance: L1d:0|2LD1ST,NUMANode:0|LOAD
                Note that items in data opt will not be filtered")
fpeakOpt = make_option(opt_str = c("-p", "--fpeak"), type = "character", default = NULL,
                       help = "Refine plot to keep only certain fpeak roofs. The option argument is a comma seperated list of the roofs.
                For instance: ADD,MUL")

statOpt = make_option(opt_str = c("-s", "--stats"), type = "logical", default=FALSE, action="store_true",
                      help = "Output deviation on bandwidths, validation and additional points into the plot.")

args = commandArgs(trailingOnly = TRUE)
optParse = OptionParser(option_list = c(inOpt, outOpt, datOpt, validOpt, titleOpt, locationOpt, bandwidthOpt, fpeakOpt, statOpt, dfilterOpt))
options = parse_args(optParse, args=args)

## options$input = "~/Documents/LARM/output/Xeon_E5-2650L_v4/joe0.roofs"
## options$data = "~/Documents/LARM/output/Xeon_E5-2650L_v4/kernels.roofs"
## options$validation = T
## options$stats = T
## options$bandwidth="NUMANode:0|2LD1ST,NUMANode:0|LOAD,NUMANode:1|2LD1ST,NUMANode:1|LOAD"
## options$filter="load|triad,load|ddot"

#################################################################################################################################
## Process options
#################################################################################################################################
if(is.null(options$input)){stop("Input option is mandatory.")}

df = read.table(options$input,header=T)
df$type=toupper(df$type)
df$location=toupper(df$location)
df$memory=toupper(df$memory)

pt = NULL
if(!is.null(options$data)){
  pt = read.table(options$data, header=T)
  pt$Location=toupper(pt$Location)
  pt$info=toupper(pt$info)
  pt$type=toupper(pt$type)
  if(!is.null(options$filter)){
    filters = unlist(strsplit(toupper(options$filter), split=","))
    new_pt = pt[0,]
    for(f in filters){
      types = unlist(strsplit(f, split="|", fixed=T))
      new_pt = rbind(new_pt, pt[pt$type==types[1] & grepl(types[2], pt$info, ignore.case=T),])
    }
    pt=new_pt
  }
}

if(is.null(options$location)){
  locations = unique(df$location)
} else{
  locations = unlist(strsplit(toupper(options$location), split=","))
}

fpeak_types=NULL
if(!is.null(options$fpeak)){fpeak_types = unlist(strsplit(toupper(options$fpeak), split=","))}

bandwidth_types = NULL
if(!is.null(options$bandwidth)){
  bandwidth_types = list()
  bs = unlist(strsplit(toupper(options$bandwidth), split=","))
  for(b in bs){bandwidth_types = c(bandwidth_types, strsplit(b, split="|", fixed=T))}
}

if(is.null(options$title)){options$title=basename(options$input)}

#################################################################################################################################
## Extract synthetic data
#################################################################################################################################

fpeak_roofs <- function(df, types=NULL, verbose=T){
  if(verbose){print("Performance peaks:")}
  fpeaks = data.frame(type=character(0), GFlop.s=numeric(0), sd=numeric(0), nthreads=integer(0), stringsAsFactors=FALSE)
  samples = df[is.na(df$memory),]
  if(is.null(types)){types = unique(samples$type)} 
  
  for(i in 1:length(types)){
    s = samples[samples$type==types[i],]
    if(nrow(s) <= 0){next}
    ns = data.frame(type=types[i], GFlop.s=median(s$GFlop.s), sd=sd(s$GFlop.s), nthreads=s$n_threads[1], stringsAsFactors=F)
    fpeaks = rbind(fpeaks,ns)
  }
  if(verbose){print(fpeaks); cat("\n")}
  fpeaks
}

bandwidth_roofs <- function(df, types=NULL, verbose=T){
  if(verbose){print("Bandwidths:")}
  bandwidths = data.frame(obj=character(0), type=character(0), GByte.s=numeric(0), sd=numeric(0), nthreads=integer(0), stringsAsFactors=FALSE)
  samples = df[df$GFlop.s==0,]
  
  if(is.null(types)){
    types = list()
    for(obj in unique(samples$memory)){
      for(type in unique(samples$type)){
        types[[length(types)+1]] = list(obj,type)
      }
    }
  }
  
  for(type in types){
    s = samples[samples$memory==type[1] & samples$type==type[2],]
    if(nrow(s) <= 0){next}
    ns = data.frame(obj=as.character(type[1]), type=as.character(type[2]), GByte.s=median(s$GByte.s), sd=sd(s$GByte.s), nthreads=s$n_threads[1], stringsAsFactors=F)
    bandwidths = rbind(bandwidths, ns)
  }
  if(verbose){print(bandwidths); cat("\n")}
  bandwidths
}

rrmse <-function(y.valid, y.roof){
  y.max = max(y.roof)
  relative_error = (y.valid-y.roof)*(y.max-y.valid)/y.max
  relative_root_mean_squared_error = sqrt(sum(relative_error*relative_error))/length(y.valid)
}

validation_points <- function(df, type){
  ret = new.env()
  ret$points = data.frame(oi=numeric(0), 
                   GFlop.s=numeric(0), 
                   sd=numeric(0), 
                   obj=character(0),
                   type=character(0),
                   stringsAsFactors=FALSE)
  
  samples = df[!is.infinite(df$Flops.Byte) & df$Flops.Byte>0 & df$type==type[2] & df$memory==type[1],]
  ois = unique(samples$Flops.Byte)
  for(oi in ois){
    s = samples[samples$Flops.Byte==oi,]
    if(nrow(s) <= 0){next}
    ns = data.frame(oi=oi, GFlop.s=median(s$GFlop.s), sd=sd(s$GFlop.s), obj=type[1], type=type[2], stringsAsFactors=FALSE)
    ret$points = rbind(ret$points,ns)
  }
  
  bandwidth = bandwidth_roofs(df, list(type), verbose=F)
  fpeak = max(df$GFlop.s)
  roof = sapply(1:nrow(ret$points), function(i){min(c(bandwidth$GByte.s[1]*ret$points$oi[i], fpeak))})
  ret$error = data.frame(memory=type[1], op_type=type[2], fpeak.GFlops.s=fpeak, error.GFlop.s=rrmse(ret$points$GFlop.s, roof), stringsAsFactors=FALSE)
  ## Error of validation points computed with: (sqrt(sum((y-y')*((y-y'max)/y'max))^2)/length(y))" 
  
  ret
}

data_points <- function(pt, types=NULL, verbose=T){
  if(is.null(pt)){return(NULL)}
  i=1
  ret = data.frame(oi=numeric(0), 
                   GFlop.s=numeric(0), 
                   sd=numeric(0), 
                   time=numeric(0),                   
                   info=character(0), 
                   type=character(0), 
                   stringsAsFactors=FALSE)
  
  if(is.null(types)){
    types = list()
    for(type in unique(pt$type)){
      for(info in unique(pt$info)){
        types[[length(types)+1]] = list(type,info)
      }
    }
  }
  
  for(type in types){
    s = pt[pt$info==type[2] & pt$type==type[1],]
    if(nrow(s) <= 0){next}
    ns = data.frame(oi=median(s$Flops/s$Bytes), 
                    GFlop.s=median(s$Flops/s$Nanoseconds), 
                    sd=sd(s$Flops/s$Nanoseconds), 
                    time=median(s$Nanoseconds),
                    info=as.character(type[2]), 
                    type=as.character(type[1]), 
                    stringsAsFactors=FALSE)
    ret = rbind(ret, ns)
  }
  
  if(verbose){print(ret); cat("\n")}
  ret
}

#################################################################################################################################
## Plot data
#################################################################################################################################

#Logarithmic sequence of points
lseq <- function(from=1, to=100000, length.out = 6) {
  exp(seq(log(from), log(to), length.out = length.out))
}

plot_bandwidths <- function(bandwidths, fpeak_max, xlim, ylim, col_start=1){
  oi = lseq(xlim[1], xlim[2], 500)
  
  for(i in 1:nrow(bandwidths)){
    GByte.s = bandwidths$GByte.s[i]
    GFlop.s = sapply(oi*GByte.s, min, fpeak_max)
    plot(oi, GFlop.s, lty=1, type="l", log="xy", axes=FALSE, xlab="", ylab="", xlim=xlim, ylim=ylim, col=col_start+i-1)
    par(new=T)
    if(options$stats){
      sd = bandwidths$sd[i]
      a0.x = oi[1];                  a0.y = oi[1]*(GByte.s-sd*0.5)
      a1.x = oi[1];                  a1.y = oi[1]*(GByte.s+sd*0.5)
      a2.x = fpeak_max/(GByte.s+sd*0.5); a2.y = fpeak_max
      a3.x = fpeak_max/(GByte.s-sd*0.5); a3.y = fpeak_max
      coord.x = c(a0.x, a1.x, a2.x, a3.x)
      coord.y = c(a0.y, a1.y, a2.y, a3.y)
      polygon(coord.x,coord.y,col=adjustcolor(col_start+i-1,alpha.f=.25), lty="blank")
      par(new=TRUE);
    }
  }
}

plot_fpeaks <- function(fpeaks, xmin){
  for(i in 1:nrow(fpeaks)){
    abline(h = fpeaks$GFlop.s[i], lty=3, col=1, lwd=2);
    axis(2, labels = fpeaks$type[i], at=fpeaks$GFlop.s[i], las=1, tick=FALSE, pos=xmin, padj=0, hadj=0)
  }
  par(new=T)
}

plot_validation <-function(pts, col=1, pch=1, cex=.5){
  points(pts$oi, pts$GFlop.s, pch=pch, col=col, cex=cex)
  par(new=T)
  if(options$stats){
    segments(x0 = pts$oi, x1 = pts$oi, y0 = pts$GFlop.s-pts$sd*0.5, y1=pts$GFlop.s+pts$sd*0.5, col=col, lty=1)
  }
}

plot_data <-function(pts, xlim, ylim, col_start=1, pch_start=1, cex=1){
  tmax = max(pts$time)
  colors = col_start+1:col_start+nrow(pts)+1
  lgd = sapply(1:nrow(pts), function(i){sprintf("%s(%s)", pts$info[i], pts$type[i])})
  
  for(i in 1:nrow(pts)){
    r = pts$time[i]*0.1/tmax
    symbols(pts$oi[i], pts$GFlop.s[i], circles=1, inches=r, add=T, fg=colors[i], bg=adjustcolor(colors[i],alpha.f=.25), xlim=xlim, ylim=ylim, lwd=.5)
    points(pts$oi[i], pts$GFlop.s[i], pch=1, col=colors[i], cex=.3)
    if(options$stats){
      segments(x0 = pts$oi[i], x1 = pts$oi[i], y0 = pts$GFlop.s[i]-pts$sd[i]*0.5, y1=pts$GFlop.s[i]+pts$sd[i]*0.5, col=colors[i], lty=1)
    }
  }
  
  legend("topright", legend=lgd, cex=.7, lty=1, col=colors, bg="white")
}

roofline_plot <- function(df, bandwidths, fpeaks, validation=F, data=NULL, verbose=T){
  #Set limit and ticks
  fpeak_max = max(fpeaks$GFlop.s)
  xmin = 2^-8; xmax = 2^6; xlim = c(xmin,xmax)
  xticks = lseq(xmin,xmax,log(xmax/xmin,base=2) + 1)
  xlabels = sapply(xticks, function(i) as.expression(bquote(2^ .(round(log(i,base=2))))))
  ymax = 10^ceiling(log10(fpeak_max)); ymin = ymax/10000; ylim = c(ymin,ymax)
  yticks = lseq(ymin, ymax, log10(ymax/ymin))
  ylabels = sapply(yticks, function(i) as.expression(bquote(10^ .(round(log10(i))))))
  
  #Plot grid
  plot(1, type="n", axes=F, xlab="", ylab="", xlim=xlim, ylim=ylim, log="xy", panel.first=abline(h=yticks, v=xticks, col="darkgray", lty = 3))
  par(new=T)
  
  #plot fpeaks
  plot_fpeaks(fpeaks,xmin)
  
  #plot bandwidths
  plot_bandwidths(bandwidths, fpeak_max, xlim, ylim)
  
  #plot validation points
  if(validation){
    errors = data.frame(memory=character(0), op_type=character(0), fpeak.GFlops.s=numeric(0), error.GFlop.s=numeric(0), stringsAsFactors=FALSE)
    for(i in 1:nrow(bandwidths)){
      valid = validation_points(df, c(bandwidths$obj[i], bandwidths$type[i]))
      plot_validation(valid$points, col=i)
      errors = rbind(errors, valid$error)
    }
    if(verbose){print(errors); cat("\n")}
  }
  
  #plot layout
  axis(1, at=xticks, labels=xlabels)
  for (i in 1:nrow(fpeaks)){
    j=1; flops = as.numeric(fpeaks$GFlop.s[i])
    while(j <= length(yticks)){
      if( (yticks[j]<2*flops) && (yticks[j]>flops) ){
        yticks = yticks[-c(j)]
        ylabels = ylabels[-c(j)]
        j = 1
      }
      else if( (yticks[j]*2>flops) && (yticks[j]<flops) ){
        yticks = yticks[-c(j)]
        ylabels = ylabels[-c(j)]
        j = 1
      }
      j = j+1
    }
  }
  yticks = c(yticks, fpeaks$GFlop.s)
  ylabels = c(ylabels, sprintf("%.2f", as.numeric(fpeaks$GFlop.s)))
  axis(2, at=yticks, labels=ylabels, las=1)
  title(main = sprintf("%s (%s)", options$title, df$location[1]), xlab="Flops/Byte", ylab="GFlops/s")
  legend("bottomright",
         legend=sprintf("%s %s=%.2fGB/s", bandwidths$obj, bandwidths$type, bandwidths$GByte.s),
         cex=.7, lty=1, col=1:nrow(bandwidths), bg="white")
  
  #plot data points
  if(!is.null(data)){plot_data(data, xlim, ylim, col_start=nrow(bandwidths), pch_start=2)}
}

for(loc in unique(df$location)){
  cat(sprintf("Process location: %s", loc), "\n\n")
  roofs = df[df$location==loc,]
  appli = NULL; if(!is.null(pt)){appli = pt[pt$Location==loc,]}
  file_output = sprintf("%s.%s", gsub(":", "-",loc), options$output)
  pdf(file_output, onefile=T, family = "Helvetica", basename(options$input), width=10, height=5)
  roofline_plot(roofs, bandwidth_roofs(roofs, types=bandwidth_types), fpeak_roofs(roofs), options$validation, data_points(appli))
  cat(sprintf("Output to %s", file_output), "\n")
  graphics.off()
}
