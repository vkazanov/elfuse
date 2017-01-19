(require 'elfuse-module)
(require 'seq)

(defconst elfuse--time-between-checks 0.01
  "Time interval in seconds between Elfuse request checks.")

(defun elfuse-start (mountpath)
  "Start Elfuse using a given MOUNTPATH."
  (interactive "DElfuse mount path: ")
  (if (elfuse--dir-mountable-p mountpath)
      (let ((abspath (file-truename mountpath)))
	(elfuse--start-loop)
	(elfuse--mount abspath))
    (message "Elfuse: %s does not exist or is not empty." mountpath)))

(defun elfuse-stop ()
  "Stop Elfuse."
  (interactive)
  (elfuse--stop))

(defun elfuse--start-loop ()
  (let ((timer (run-at-time
                nil
                elfuse--time-between-checks
                (lambda ()
		  (unless (elfuse--check-callbacks)
		    (cancel-timer timer))))))))


(defun elfuse--dir-mountable-p (path)
  (and (file-exists-p path)
       ;; only . and ..
       (= (length (directory-files path)) 2)))

(defun elfuse--create-callback (path)
  (message "CREATE: %s" path)
  (if (member oldpath '("/hello" "/other" "/etc")) -1 1))

(defun elfuse--rename-callback (oldpath newpath)
  (message "RENAME: %s" oldpath newpath)
  (if (member oldpath '("/hello" "/other" "/etc")) 1 -1))

(defun elfuse--readdir-callback (path)
  (message "READDIR: %s" path)
  ["." ".." "hello" "other" "etc"])

(defun elfuse--getattr-callback (path)
  (message "GETATTR: %s" path)
  (cond
   ((equal path "/hello") (vector 'file (seq-length "hellodata")))
   ((equal path "/other") (vector 'file (seq-length "otherdata")))
   ((equal path "/etc") (vector 'file (seq-length "etcdata")))
   ((equal path "/") (vector 'dir 0))
   (t [nil, 0])))

(defun elfuse--open-callback (path)
  (message "OPEN: %s" path)
  (cond
   ((equal path "/hello") t)
   ((equal path "/other") t)
   ((equal path "/etc") t)
   (t nil)))

(defun elfuse--release-callback (path)
  (message "RELEASE: %s" path)
  (elfuse--open-callback path))

(defun elfuse--read-callback (path offset size)
  (message "READ: %s %d %d" path offset size)
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
  (message "WRITE: %s %s %d" path buf offset)
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

