
xmq.pdf: xmq.tex
	xelatex --halt-on-error xmq.tex
	rail xmq
	xelatex --halt-on-error xmq.tex

view_doc:
	man -l xq.1

clean:
	rm -f  xmq.rai xmq.rao xmq.aux xmq.log xmq.pdf
