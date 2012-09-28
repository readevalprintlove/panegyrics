# bacwn

Deep studies in Clojure, including but not limited to:

* Datalog (in `bacwn.datalog`)
* Cells
* Hygienic macros
* Graph theory
* Wavelet transforms
* Term-rewriting
* Macrology
* Prolog
* Blackboard systems
* Problem solvers
* OO
* AI
* Dataflow
* ...

Anything useful will eventually find its way into a separate libarary.

## Datalog

The Bacwn Datalog library is based on the old Clojure-contrib datalog implementation.  The library's syntax will change over time and it will be made to conform to modern Clojure's, but the spirit of the original will remain in tact.  To use Bacwn Datalog in your onw libraries, add the following to your dependencies:

### Leiningen

    :dependencies [[bacwn "0.1.0"] ...]    

### Maven

Add the following to your `pom.xml` file:

    <dependency>
      <groupId>evalive</groupId>
      <artifactId>bacwn</artifactId>
      <version>0.1.0</version>
    </dependency>

## License

Copyright (C) 2012 Fogus

Distributed under the Eclipse Public License, the same as Clojure.
