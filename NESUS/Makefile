.PHONY: all force clean cleanall

TARGET=roofline_model_extension

all: $(TARGET).pdf abstract.pdf

abstract.pdf: abstract.tex
	pdflatex $<

$(TARGET).pdf: $(TARGET).tex
	pdflatex $(TARGET)
	bibtex $(TARGET)
	pdflatex $(TARGET)
	pdflatex $(TARGET)

clean:
	rm -f *.log *.out *.aux *.bbl *.blg
