(require 'elfuse)

(elfuse-define-op readdir (path)
  (message "READDIR: %s" path)
  (unless (equal path "/")
    (signal 'elfuse-op-error elfuse-errno-ENOENT))
  ["." ".." "hello" "other" "etc"])

(elfuse-define-op getattr (path)
  (message "GETATTR: %s" path)
  (cond
   ((equal path "/hello") (vector 'file (seq-length "hellodata")))
   ((equal path "/other") (vector 'file (seq-length "otherdata")))
   ((equal path "/etc") (vector 'file (seq-length "etcdata")))
   ((equal path "/") (vector 'dir 0))
   (t (signal 'elfuse-op-error elfuse-errno-ENOENT))))

(elfuse-define-op open (path)
  (message "OPEN: %s" path)
  (cond
   ((equal path "/hello") t)
   ((equal path "/other") t)
   ((equal path "/etc") t)
   (t (signal 'elfuse-op-error elfuse-errno-ENOENT))))

(elfuse-define-op read (path offset size)
  (message "READ: %s %d %d" path offset size)
  (cond
   ((equal path "/hello") (hello--substring "hellodata" offset size))
   ((equal path "/other") (hello--substring "otherdata" offset size))
   ((equal path "/etc") (hello--substring "etcdata" offset size))
   (t (signal 'elfuse-op-error elfuse-errno-ENOENT))))

(defun hello--substring (str offset size)
  (cond
   ((> offset (seq-length str)) "")
   ((> (+ offset size) (seq-length str)) (seq-subseq str offset))
   (t (seq-subseq str offset (+ offset size)))))
