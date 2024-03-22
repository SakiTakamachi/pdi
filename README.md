# pdi
Pdi - Dependency injection

# How to try dev-version?

```
$ cd pdi
$ phpize
$ ./configure
$ make install
```

Then enable pdi in php.ini.
e.g.
```
extension=/pdi/modules/pdi.so
```

# Sample code

```
<?php

class Baz{}

interface Foo {}
class Bar implements Foo{
    public function __construct(Baz $baz) {}
}

$di = new Pdi();
$di->bind(Foo::class, Bar::class);

$barInstance = $di->make(Foo::class);
```
