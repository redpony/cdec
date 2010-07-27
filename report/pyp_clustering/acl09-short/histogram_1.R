type = matrix(c(1,0,0, 0,0,0, 0,0,0), nr=3)
dimnames(type)=list(Type=c("0", "1", "2"),Constituent=c("the","cats","meow")) 

pdf('histogram_1.pdf', paper="special", width=18, height=10, onefile=FALSE,pointsize=40)

barplot(type, axes=FALSE, space=c(0.5,0.5,0.5,2.0,0.5,0.5,2.0,0.5,0.5), xpd=F,beside=TRUE,
legend=rownames(type), ylab="Tables", ylim=range(0,2), col=c("black","grey","darkblue"), las=1)
axis(side=2, at=c(0,1,2))
