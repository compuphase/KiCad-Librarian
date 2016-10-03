# KiCad-Librarian
The KiCad Librarian allows you to view footprints and schematic symbols in KiCad 
libraries, and move or copy footprints & symbols from one library to another.
It allows you to scan all libraries in a list, so that you can check for duplicates
or naming conflicts. It supports a repository for central storage (and sharing)
of symbols and footprints.

A run-through of the Librarian is posted on YouTube: https://www.youtube.com/watch?v=sDICxGqsZC8

Parameters for a footprint, such as pad sizes, shapes and pitch, can be adjusted
by changing values in a property table. This works for new footprints (created
from one of several templates) and for _existing_ footprints (even those
that were not initially created by the Librarian). If a template is present, the
Librarian also creates (or adjusts) a 3D model for the package.

With the goal of consistency in a library of footprints, the Librarian calculates
essential metrics of pad sizes, spans and pitch. It provides a comparison mode
where you can overlay two footprints for a visual comparison of the parameters.
(A visual comparison of schematic symbols is also supported.)

For symbols, the pin types, shapes and positions can be adjusted. In the same
way as for footprints, you can create new symbols from a template. Pin numbers and
descriptions can be imported from the clipboard and/or conveniently edited
in a table.

Additionally, the KiCad Librarian can create a report of all parts in a library.
The reports contain the essential statistics of the parts. For footprints, this
includes a drawing of the footprint and the pitch and pad size parameters; for
symbols, it includes alias names and the list of applicable footprints.

With KiCad alone, you can do all of the above, too (except for the reports). This
Librarian was developed to make managing the libraries more convenient. For example,
instead of needing to "export" a symbol from one library and then to "import" it
into the other, the KiCad Librarian allows you to open two libraries at once, and to
copy the symbol from one into the other in a single operation. Another example: if
you want to make the pads of a 48-pin QFP a tenth of a millimetre narrower, you need
only to adjust a single value to change all 48 pads at once.

Footprint library management now also means footprint conversion. KiCad footprints
exist in three formats (legacy-mils, legacy-mm and s-expressions), and all three formats are used interchangeably. When moving
a footprint from one library to another, the footprint data may need to be
converted. The Librarian does this automatically while copying or moving
footprints.
