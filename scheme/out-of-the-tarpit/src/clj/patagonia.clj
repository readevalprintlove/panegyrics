(ns patagonia)

(defprotocol Sys
  (module [this])
  (identifier [this]))

(defprotocol Lifecycle
  (startup [this])
  (pause [this])
  (resume [this])
  (shutdown [this]))

(defprotocol Store)
(defprotocol State)

(defprotocol Precepts
  (propagate [this]))

(defprotocol Suppressor
  (suppress [this]))

(defprotocol Scheduler)


(defprotocol Message)

(defprotocol Sink)
(defprotocol Source)

(defprotocol Fact)
(defprotocol Derivation)

(defprotocol CAS)
(defprotocol Sequencer)

(defprotocol Control)
(defprotocol Feeder)
(defprotocol Observer)
