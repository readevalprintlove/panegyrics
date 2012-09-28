(ns bacwn.datalwg
  (:use [bacwn.datalog.impl.database :only (make-database add-tuples)]))

(defn explode
  "Convert a map into a clj-Datalog tuple vector. Brittle, but
   works along the happy path."
  [entity]
  (let [relation-type (-> entity seq ffirst namespace keyword)
        id-key (keyword (name relation-type) "db.id")
        id  (get entity id-key)
        kvs (seq (dissoc entity id-key))]
    (vec
     (apply concat [relation-type id-key id]
            (reduce (fn [acc [k v]]
                      (cons [(keyword (name k)) v] acc))
                    []
                    kvs)))))

(defmacro facts [db & tuples]
  `(add-tuples ~db
    ~@(map explode tuples)))

(defn q
  [query db rules bindings]
  (run-work-plan
   (build-work-plan rules query)
   db
   bindings))
