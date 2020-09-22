# d2tk

## Data Driven Tool Kit

A performant, dyamic, immediate-mode GUI tool kit in C which partially renders
on-change only by massively hashing-and-cashing of vector drawing instructions
and on-demand rendered sprites.

### Build / test

	git clone https://git.open-music-kontrollers.ch/lad/d2tk
	cd d2tk 
	meson build
	cd build
	ninja -j4

#### Pugl/NanoVG backend

	./d2tk.nanovg

#### Pugl/Cairo backend

	./d2tk.cairo

#### FBdev/Cairo backend

	./d2tk.fbdev

### Screenshots

![Screenshot 1](/screenshots/screenshot_1.png)

![Screenshot 2](/screenshots/screenshot_2.png)

![Screenshot 3](/screenshots/screenshot_3.png)

![Screenshot 4](/screenshots/screenshot_4.png)

![Screenshot 5](/screenshots/screenshot_5.png)

![Screenshot 6](/screenshots/screenshot_6.png)

![Screenshot 7](/screenshots/screenshot_7.png)

![Screenshot 8](/screenshots/screenshot_8.png)

### License

Copyright (c) 2018-2019 Hanspeter Portner (dev@open-music-kontrollers.ch)

This is free software: you can redistribute it and/or modify
it under the terms of the Artistic License 2.0 as published by
The Perl Foundation.

This source is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
Artistic License 2.0 for more details.

You should have received a copy of the Artistic License 2.0
along the source as a COPYING file. If not, obtain it from
<http://www.perlfoundation.org/artistic_license_2_0>.
