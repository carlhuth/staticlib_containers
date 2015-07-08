Staticlibs containers library
=============================

This project is a part of [Staticlibs](http://staticlibs.net/).

This project contains implementation of the following containers:

 - `producer_consumer_queue` single producer single consumer non-blocking queue implementation
from [facebook/folly](https://github.com/facebook/folly/blob/b75ef0a0af48766298ebcc946dd31fe0da5161e3/folly/ProducerConsumerQueue.h) with cosmetic chages
 - `blocking_queue` optionally bounded growing FIFO blocking queue with support for blocking and 
non-blocking multiple consumers and always non-blocking multiple producers

This library is header-only.

Link to [API documentation](http://staticlibs.github.io/staticlib_containers/docs/html/namespacestaticlib_1_1containers.html).

License information
-------------------

This project is released under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0)

Changelog
---------

**2015-07-05**

* 1.1.0, added `poll` with timeout to `blocking_queue`

**2015-07-05**

 * 1.0.0, initial public version
