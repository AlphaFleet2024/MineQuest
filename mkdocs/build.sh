#!/bin/sh -e

# Patch Pygments and Python-Markdown
PYGMENTS_FILE=$(pip show pygments | awk '/Location/ { print $2 }')/pygments/formatters/html.py
MARKDOWN_FILE=$(pip show markdown | awk '/Location/ { print $2 }')/markdown/extensions/codehilite.py
patch -N -r - $PYGMENTS_FILE code_tag.patch || true
patch -N -r - $MARKDOWN_FILE lua_highlight.patch || true

# Split lua_api.txt on top level headings
cat ../doc/lua_api.txt | csplit -sz -f docs/section - '/^=/-1' '{*}'

cat > mkdocs.yml << EOF
site_name: Minetest API Documentation
theme:
    name: readthedocs
    highlightjs: False
extra_css:
    - css/code_styles.css
    - css/extra.css
markdown_extensions:
    - toc:
        permalink: True
    - codehilite
plugins:
    - search:
        separator: '[\s\-\.\(]+'
nav:
- "Home": index.md
EOF

mv docs/section00 docs/index.md

for f in docs/section*
do
	title=$(head -1 $f)
	fname=$(echo $title | tr '[:upper:]' '[:lower:]')
	fname=$(echo $fname | sed 's/ /-/g')
	fname=$(echo $fname | sed "s/'//g").md
	mv $f docs/$fname
	echo "- \"$title\": $fname" >> mkdocs.yml
done

mkdocs build --site-dir ../public
