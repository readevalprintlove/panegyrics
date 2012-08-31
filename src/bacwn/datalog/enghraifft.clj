(ns bacwn.datalog.enghraifft
  (:use [bacwn.datalwg :as bacwn]
        [bacwn.datalog :only (build-work-plan run-work-plan)]
        [bacwn.datalog.impl.rules :only (<- ?- rules-set)]
        [bacwn.datalog.impl.database :only (make-database add-tuples)]
        [bacwn.datalog.impl.util :only (*trace-datalog*)]))

(def mst3k-schema
  (make-database
   (relation :character [:character/db.id :name :human?])
   (index :character :name)

   (relation :location [:location/db.id :character :name])
   (index :location :name)))

(def mst3k-db
  (-> mst3k-schema
      (facts {:character/db.id 0 :character/name "Joel" :character/human? true}
             {:character/db.id 1 :character/name "Crow" :character/human? false}
             {:character/db.id 2 :character/name "TV's Frank" :character/human? true}
             {:location/db.id  0 :location/character 0 :location/name "SoL"}
             {:location/db.id  0 :location/character 1 :location/name "SoL"}
             {:location/db.id  1 :location/character 2 :location/name "Gizmonics"})))

(def rules
  (rules-set
   (<- (:stationed-at :location/name ?loc-name :character/name ?char-name)
       (:location  :location/name ?loc-name :location/character ?char)
       (:character :character/db.id ?char :character/name ?char-name))))

(def wp-1 (build-work-plan rules (?- :stationed-at :location/name '??loc :character/name ?char-name)))
(run-work-plan wp-1 mst3k-db {'??loc "SoL"})


(def rules2
  (rules-set
   (<- (:works-for :employee ?x :boss ?y) (:boss :employee-id ?e-id :boss-id ?b-id)
       (:employee :id ?e-id :name ?x)
       (:employee :id ?b-id :name ?y))
   (<- (:works-for :employee ?x :boss ?y) (:works-for :employee ?x :boss ?z)
       (:works-for :employee ?z :boss ?y))
   (<- (:employee-job* :employee ?x :job ?y) (:employee :name ?x :position ?pos)
       (:can-do-job :position ?pos :job ?y))
   (<- (:employee-job* :employee ?x :job ?y) (:job-replacement :job ?y :can-be-done-by ?z)
       (:employee-job* :employee ?x  :job ?z))
   (<- (:employee-job* :employee ?x :job ?y) (:can-do-job :job ?y)
       (:employee :name ?x :position ?z)
       (if = ?z :boss))
   (<- (:employee-job :employee ?x :job ?y) (:employee-job* :employee ?x :job ?y)
       (:employee :id ?id :name ?x)
       (not! :job-exceptions :id ?id :job ?y))
   (<- (:bj :name ?x :boss ?y) (:works-for :employee ?x :boss ?y)
       (not! :employee-job :employee ?y :job :pc-support))))