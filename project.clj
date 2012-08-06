(defproject bacwn "0.1.0"
  :description "Deep studies in Clojure."
  :dependencies [[org.clojure/clojure "1.5.0-master-SNAPSHOT"]]
  :dev-dependencies [[lein-marginalia "0.7.1"]
                     [lein-multi "1.1.0"]]
  :plugins [[lein-swank "1.4.4"]]
  :multi-deps {"1.2" [[org.clojure/clojure "1.2.0"]]
               "1.3" [[org.clojure/clojure "1.3.0"]]
               "1.4" [[org.clojure/clojure "1.4.0"]]
               "1.5" [[org.clojure/clojure "1.5.0-master-SNAPSHOT"]]}
  :repositories {"sonatype-oss-public" "https://oss.sonatype.org/content/groups/public/"})

