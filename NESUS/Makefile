.PHONY: all force clean cleanall

TARGET=roofline_model_extension

all: $(TARGET).pdf

$(TARGET).pdf: $(TARGET).tex
	pdflatex $(TARGET)
	bibtex $(TARGET)
	pdflatex $(TARGET)
	pdflatex $(TARGET)

clean:
	rm -f $(TARGET).pdf *.log *.out *.aux *.bbl *.blg
