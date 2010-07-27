%wsj_lengths = load([ 'wsj_lengths.dat']);
%save([ 'wsj_lengths.mat'],'wsj_lengths');
load wsj
load wsj_lengths

figure(1)
clf 

hold on

%colors = [0 0 0; 0 0 1; 1 0 0; 0 1 0]; %pure black, red, blue, green
colors = [0 0 0; 1 .4 .2; .4 .4 1; 0 .7 .5]; %same but less garish
%colors = [0 0 0; .6 .4 .4; .9 .6 .6; 1 .8 .8]; %shades of pink
%colors = [0 0 0; .3 .3 1; .4 .8 1; .5 1 .8]; %blue/green

for i = 9-[3:6]

  b = 10^(i-1)
   
  disp(['Loading results for b = ' num2str(b) ]);
  %%%  uncomment these lines if .mat file is not yet generated. %%%
  %typecountrecord= load([ 'outputs/typecountrecordwsjflat0.0.' num2str(b) '.0.dat']);
  %typecountrecordmean = mean(typecountrecord(:,:));
  %save([ 'outputs/typecountrecordmeanwsjflat0.0.' num2str(b) '.0.mat'],'typecountrecordmean');
  load([ 'outputs/typecountrecordmeanwsjflat0.0.' num2str(b) '.0.mat']);
  
  %plot emprical counts with error bars
  [logbins meanval seval] = logbinmean(counts,typecountrecordmean,20,20);
  ph = plot(log10(logbins),log10(meanval));
  set(ph,'color',colors(i-2,:),'linestyle','o','linewidth',2,'markersize',10);
  %errorbar(log10(logbins),log10(meanval),log10(meanval+seval)-log10(meanval),log10(meanval-seval)-log10(meanval),'k.');

end

set(gca,'xtick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'ytick',log10([.1:.1:1 2:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'xlim',[-0.1 3.5])
set(gca,'ylim',[-.1 2])
set(gca,'FontSize',16)
set(gca,'xticklabel', {'1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
set(gca,'yticklabel', {'0.1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
    '1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
%title('Chinese restaurant process adaptor')
ylabel('Mean number of lexical entries (tables)')
xlabel('Word frequency (n_w)')
labs = {'\alpha = 100000','\alpha = 10000','\alpha = 1000','\alpha = 100'};
legend(labs,'Location','NorthWest')
box on
