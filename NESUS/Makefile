PAPER=roofline_model_extension

$(PAPER).pdf: $(PAPER).tex
	pdflatex $(PAPER).tex
	bibtex $(PAPER)
	pdflatex $(PAPER).tex
	pdflatex $(PAPER).tex

clean:
	rm -f *.log *.out *.aux *.bbl *.blg

cleanall: clean
	rm -f abstract.pdf $(PAPER).pdf

