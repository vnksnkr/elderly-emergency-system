 // get selected row
 // display selected row data in text input
            
 var rIndex,
 table = document.getElementById("table");

// check the empty input
function checkEmptyInput()
{
 var isEmpty = false,
     sno = document.getElementById("sno").value,
     diagnosis = document.getElementById("diagnosis").value,
     medication = document.getElementById("medication").value,
     dosage = document.getElementById("dosage").value;
     duration = document.getElementById("duration").value;
if(sno === ""){
    alert("Sno Cannot Be Empty");
    isEmpty = true;
}
 else if(diagnosis === ""){
     alert("Diagnosis Cannot Be Empty");
     isEmpty = true;
 }
 else if(medication === ""){
     alert("Medication  Cannot Be Empty");
     isEmpty = true;
 }
 else if(dosage === ""){
     alert("Dosage cannot Be Empty");
     isEmpty = true;
 }
 else if(duration === ""){
     alert("Duration cannot Be Empty");
     isEmpty = true;
 }
 return isEmpty;
}

// add Row
function addHtmlTableRow()
{
 // get the table by id
 // create a new row and cells
 // get value from input text
 // set the values into row cell's
 if(!checkEmptyInput()){
 var newRow = table.insertRow(table.length),
     cell1 = newRow.insertCell(0),
     cell2 = newRow.insertCell(1),
     cell3 = newRow.insertCell(2),
     cell4 = newRow.insertCell(3),
     cell5 = newRow.insertCell(4),
     sno = document.getElementById("sno").value,
     diagnosis = document.getElementById("diagnosis").value,
     medication = document.getElementById("medication").value,
     dosage = document.getElementById("dosage").value;
     duration = document.getElementById("duration").value;


 cell1.innerHTML = sno;
 cell2.innerHTML = diagnosis;
 cell3.innerHTML = medication;
 cell4.innerHTML = dosage;
 cell5.innerHTML = duration;
 // call the function to set the event to the new row
 selectedRowToInput();
}
}

// display selected row data into input text
function selectedRowToInput()
{
 
 for(var i = 1; i < table.rows.length; i++)
 {
     table.rows[i].onclick = function()
     {
       // get the seected row index
       rIndex = this.rowIndex;
       document.getElementById("sno").value = this.cells[0].innerHTML;
       document.getElementById("diagnosis").value = this.cells[1].innerHTML;
       document.getElementById("medication").value = this.cells[2].innerHTML;
       document.getElementById("dosage").value = this.cells[3].innerHTML;
       document.getElementById("duration").value = this.cells[4].innerHTML;
     };
 }
}
selectedRowToInput();

function editHtmlTbleSelectedRow()
{
 var sno = document.getElementById("sno").value,
     diagnosis = document.getElementById("diagnosis").value,
     medication = document.getElementById("medication").value;
     dosage = document.getElementById("dosage").value;
     duration = document.getElementById("duration").value;
if(!checkEmptyInput()){
 table.rows[rIndex].cells[0].innerHTML = sno;
 table.rows[rIndex].cells[1].innerHTML = diagnosis;
 table.rows[rIndex].cells[2].innerHTML = medication;
 table.rows[rIndex].cells[3].innerHTML = dosage;
 table.rows[rIndex].cells[4].innerHTML = duration;
}
}

function removeSelectedRow()
{
 table.deleteRow(rIndex);
 // clear input text
 document.getElementById("sno").value = "";
 document.getElementById("diagnosis").value = "";
 document.getElementById("medication").value = "";
 document.getElementById("dosage").value = "";
 document.getElementById("duration").value = "";
}