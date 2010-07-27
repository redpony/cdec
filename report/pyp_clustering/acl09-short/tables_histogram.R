pdf('tables_histogram.pdf', paper="special", width=5, height=5, onefile=FALSE,pointsize=15)
barplot(c(0,1,2), names.arg=c(0,1,2), width=0.5, space=1.0, xpd=T, beside=TRUE, axes=F, ylab="Frequency", ylim=range(0,2), col=c("black"), las=1, xlab="Number of customers at table")
axis(side=2, at=c(0,1,2))
#axis(side=1)
