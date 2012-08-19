(ns bacwn.datalwg)

(defn explode
  "Convert a map into a clj-Datalog tuple vector. Brittle, but
   works along the happy path."
  [entity]
  (let [id  (:db/id entity)
        kvs (seq (dissoc entity :db/id))
        relation-type (-> kvs ffirst namespace keyword)]
    (reduce (fn [coll ins]
              (apply conj coll ins))
            [relation-type :db/id id] 
            (reduce (fn [acc [k v]]
                      (cons [(keyword (name k)) v] acc))
                    []
                    kvs))))


(comment

  (explode
   {:db/id 1
    :employee/name "Bob"
    :employee/position :boss})
)

(comment

(def db
  (add-tuples
   db-base
   [:employee :id 1  :name "Bob"    :position :boss]
   [:employee :id 2  :name "Mary"   :position :chief-accountant]
   [:employee :id 3  :name "John"   :position :accountant]
   [:employee :id 4  :name "Sameer" :position :chief-programmer]
   [:employee :id 5  :name "Lilian" :position :programmer]
   [:employee :id 6  :name "Li"     :position :technician]
   [:employee :id 7  :name "Fred"   :position :sales]
   [:employee :id 8  :name "Brenda" :position :sales]
   [:employee :id 9  :name "Miki"   :position :project-management]
   [:employee :id 10 :name "Albert" :position :technician]
   
   [:boss :employee-id 2  :boss-id 1]
   [:boss :employee-id 3  :boss-id 2]
   [:boss :employee-id 4  :boss-id 1]
   [:boss :employee-id 5  :boss-id 4]
   [:boss :employee-id 6  :boss-id 4]
   [:boss :employee-id 7  :boss-id 1]
   [:boss :employee-id 8  :boss-id 7]
   [:boss :employee-id 9  :boss-id 1]
   [:boss :employee-id 10 :boss-id 6]
   
   [:can-do-job :position :boss               :job :management]
   [:can-do-job :position :accountant         :job :accounting]
   [:can-do-job :position :chief-accountant   :job :accounting]
   [:can-do-job :position :programmer         :job :programming]
   [:can-do-job :position :chief-programmer   :job :programming]           
   [:can-do-job :position :technician         :job :server-support]
   [:can-do-job :position :sales              :job :sales]
   [:can-do-job :position :project-management :job :project-management]
   
   [:job-replacement :job :pc-support :can-be-done-by :server-support]
   [:job-replacement :job :pc-support :can-be-done-by :programming]
   [:job-replacement :job :payroll    :can-be-done-by :accounting]
   
   [:job-exceptions :id 4 :job :pc-support]))
  
)