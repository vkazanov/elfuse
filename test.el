(require 'elfuse)
(require 'seq)

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

(defun elfuse--read-callback (path offset size)
  (cond
   ((equal path "/hello") (elfuse--read-substring "hellodata" offset size))
   ((equal path "/other") (elfuse--read-substring "otherdata" offset size))
   ((equal path "/etc") (elfuse--read-substring "etcdata" offset size))
   (t nil)))

(defun elfuse--read-substring (str offset size)
  (cond
   ((> offset (seq-length str)) "")
   ((> (+ offset size) (seq-length str)) (seq-subseq str offset))
   (t (seq-subseq str offset (+ offset size)))))

(elfuse--start "mount/")

(let ((timer (run-at-time
              nil
              0.01
              (lambda () (let ((check-p (elfuse--check-callbacks)))
                      (unless check-p
                        (cancel-timer timer))))))) )

;; (elfuse--stop)
