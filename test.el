(require 'elfuse-module)
(require 'seq)

(defun elfuse--create-callback (path)
  (if (member oldpath '("/hello" "/other" "/etc")) -1 1))

(defun elfuse--rename-callback (oldpath newpath)
  (if (member oldpath '("/hello" "/other" "/etc")) 1 -1))

(defun elfuse--readdir-callback (path)
  ["." ".." "hello" "other" "etc"])

(defun elfuse--getattr-callback (path)
  (cond
   ((equal path "/hello") (vector 'file (seq-length "hellodata")))
   ((equal path "/other") (vector 'file (seq-length "otherdata")))
   ((equal path "/etc") (vector 'file (seq-length "etcdata")))
   ((equal path "/") (vector 'dir 0))
   (t [nil, 0])))

(defun elfuse--open-callback (path)
  (cond
   ((equal path "/hello") t)
   ((equal path "/other") t)
   ((equal path "/etc") t)
   (t nil)))

(defun elfuse--release-callback (path)
  (elfuse--open-callback path))

(defun elfuse--read-callback (path offset size)
  (cond
   ((equal path "/hello") (elfuse--substring "hellodata" offset size))
   ((equal path "/other") (elfuse--substring "otherdata" offset size))
   ((equal path "/etc") (elfuse--substring "etcdata" offset size))
   (t nil)))

(defun elfuse--substring (str offset size)
  (cond
   ((> offset (seq-length str)) "")
   ((> (+ offset size) (seq-length str)) (seq-subseq str offset))
   (t (seq-subseq str offset (+ offset size)))))

(defun elfuse--write-callback (path buf offset)
  (message "WRITE: %s %d" buf offset)
  (cond
   ((equal path "/hello") 1)
   ((equal path "/other") 2)
   ((equal path "/etc") 3)
   (t 0)))

(defun elfuse--truncate-callback (path offset)
  (message "TRUNCATE: %s %d" path offset)
  (cond
   ((equal path "/hello") 0)
   ((equal path "/other") 0)
   ((equal path "/etc") 0)
   (t -1)))


(defun elfuse--loop ()
  (let ((timer (run-at-time
                nil
                0.01
                (lambda () 
		  (unless (elfuse--check-callbacks)
		    (cancel-timer timer))))))))

(elfuse--start "mount/")
;; (elfuse--loop)
;; (elfuse--stop)
;; (elfuse--start "mount/")
;; (elfuse--loop)
