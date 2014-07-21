Peak Power Solar Charger
========================

This projects implements a peak power charger for lead-acid batteries.
It is inspired by a very similar project for the Arduino platform by Tim
Nolan who has extensive documentation on his website `www.timnolan.com`_

.. _`www.timnolan.com`: http://www.timnolan.com

In addition to Tim Nolans project, my design makes the current
measurements (battery voltage, solar voltage, solar current and solar
watts) available via coap resources.

I'm also making kicad drawings and board layout available, see the
``kicad`` directory. Note that the board was never manufactured so far,
I've built a breadboard. The breadboard design is in an inkscape drawing
with several layers in ``layout.svg``.

Ralf Schlatterbeck
